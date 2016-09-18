/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 *
 *   Copyright (C) 2007 UChicago/Argonne LLC
 *   See COPYRIGHT notice in top-level directory.
 *
 *   Copyright (C) 2014-2016 Seagate Systems UK Ltd.
 */

#include "adio.h"

/* Generic version of a "collective open".  Assumes a "real" underlying
 * file system (meaning no wonky consistency semantics like NFS).
 *
 * optimization: by having just one process create a file, close it,
 * then have all N processes open it, we can possibly avoid contention
 * for write locks on a directory for some file systems.
 *
 * Happy side-effect: exclusive create (error if file already exists)
 * just falls out
 *
 * Note: this is not a "scalable open" (c.f. "The impact of file systems
 * on MPI-IO scalability").
 */

void ADIOI_GEN_OpenColl(ADIO_File fd, int rank,
	int access_mode, int *error_code)
{
    int orig_amode_excl, orig_amode_wronly, max_error_code;
    int myrank;
    MPI_Comm tmp_comm;

    orig_amode_excl = access_mode;

    MPI_Comm_rank(fd->comm, &myrank);

    /* check cache mode */
    if (fd->hints->e10_cache == ADIOI_HINT_ENABLE) {
	/* obtain MPI_File handle for cache file and initialize it */
	MPI_File mpi_fh = MPIO_File_create(sizeof(struct ADIOI_FileD));
	fd->cache_fd = MPIO_File_resolve(mpi_fh);
	fd->cache_fd->cookie = ADIOI_FILE_COOKIE;
	fd->cache_fd->fp_ind = fd->fp_ind;
	fd->cache_fd->fp_sys_posn = fd->fp_sys_posn;
	fd->cache_fd->comm = MPI_COMM_SELF;
	fd->cache_fd->filename = ADIOI_GetCacheFilePath(fd->filename, fd->hints->e10_cache_path);
	fd->cache_fd->file_system = fd->file_system;
	fd->cache_fd->fs_ptr = fd->fs_ptr;
	fd->cache_fd->fns = fd->fns;
	fd->cache_fd->disp = fd->disp;
	fd->cache_fd->split_coll_count = fd->split_coll_count;
	fd->cache_fd->cache_fd = ADIO_FILE_NULL;
	fd->cache_fd->atomicity = fd->atomicity;
	fd->cache_fd->etype = fd->etype;
	fd->cache_fd->filetype = fd->filetype;
	fd->cache_fd->etype_size = fd->etype_size;
	fd->cache_fd->file_realm_st_offs = fd->file_realm_st_offs;
	fd->cache_fd->file_realm_types = fd->file_realm_types;
	fd->cache_fd->perm = fd->perm;
	fd->cache_fd->async_count = fd->async_count;
	fd->cache_fd->fortran_handle = fd->fortran_handle;
	fd->cache_fd->err_handler = fd->err_handler;
	fd->cache_fd->info = MPI_INFO_NULL;
	fd->cache_fd->agg_comm = MPI_COMM_NULL;
	fd->cache_fd->access_mode = ADIO_CREATE | ADIO_RDWR;

	/* check discard flag and set delete on close for cache file */
	if (fd->hints->e10_cache_discard_flag == ADIOI_HINT_ENABLE) {
	    fd->cache_fd->access_mode |= ADIO_DELETE_ON_CLOSE;
	}

	/* every process tries opening the cache file */
	(*(fd->fns->ADIOI_xxx_Open))(fd->cache_fd, error_code);

	/* get name for this node */
	char nodename[32];
        gethostname(nodename, 32);

    	/* if file exists disable create and reopen */
	if (*error_code == MPI_ERR_FILE_EXISTS) {
	    FPRINTF(stderr, " -> %s already exists in node %s\n",
		    fd->cache_fd->filename, nodename);
	    fd->cache_fd->access_mode ^= ADIO_CREATE;
	    (*(fd->fns->ADIOI_xxx_Open))(fd->cache_fd, error_code);
	}
	/* NOTE: if there are many processes per node one will create
	 *       the file and the others will return error EEXIST.
	 *       These must reopen the file without the create flag,
	 *       meaning that for every node only one proc will have
	 *       ADIO_CREATE flag on. This will also be used to delete
	 *       the cache file using only one process.
	 */

	if (*error_code != MPI_SUCCESS)
	    FPRINTF(stderr, " -> proc %d error while opening cache file %s in node %s\n",
	            myrank, fd->cache_fd->filename, nodename);

	/* init synchronisation thread pool */
	if (*error_code == MPI_SUCCESS && fd->hints->e10_cache_flush_flag != ADIOI_HINT_FLUSHNONE) {
	    if (fd->hints->cb_write == ADIOI_HINT_ENABLE)
		if (fd->is_agg)
		    *error_code = ADIOI_Sync_thread_pool_init(fd, NULL);
		else
		    *error_code = MPI_SUCCESS;
	    else
		*error_code = ADIOI_Sync_thread_pool_init(fd, NULL);

	    if (*error_code != MPI_SUCCESS)
		FPRINTF(stderr, " -> proc %d error while creating sync thread pool in node %s\n",
			myrank, nodename);
	}

	/* check every process has returned success */
	MPI_Allreduce(error_code, &max_error_code, 1, MPI_INT, MPI_MAX, fd->comm);

	if (max_error_code != MPI_SUCCESS) {
	    if (*error_code == MPI_SUCCESS ||
		    *error_code == ADIOI_ERR_THREAD_CREATE) {
		(*(fd->fns->ADIOI_xxx_Close))(fd->cache_fd, error_code);
		if ((fd->cache_fd->access_mode & ADIO_CREATE) &&
			(fd->cache_fd->access_mode & ADIO_DELETE_ON_CLOSE)) {
		    ADIO_Delete(fd->cache_fd->filename, error_code);
		}
                /* fini synchronisation thread pool */
		if (fd->hints->e10_cache_flush_flag != ADIOI_HINT_FLUSHNONE && fd->thread_pool) {
		    ADIOI_Sync_thread_pool_fini(fd);
		}
	    }

	    /* Revert to standard file access: no cache */
	    ADIOI_Info_set(fd->info, "e10_cache", "disable");
	    fd->hints->e10_cache = ADIOI_HINT_DISABLE;
	}
	else {
	    fd->cache_fd->is_open = 1;
	}
    }

    if (access_mode & ADIO_CREATE) {
       if(rank == fd->hints->ranklist[0]) {
	   /* remove delete_on_close flag if set */
	   if (access_mode & ADIO_DELETE_ON_CLOSE)
	       fd->access_mode = access_mode ^ ADIO_DELETE_ON_CLOSE;
	   else
	       fd->access_mode = access_mode;

	   tmp_comm = fd->comm;
	   fd->comm = MPI_COMM_SELF;
	   (*(fd->fns->ADIOI_xxx_Open))(fd, error_code);
	   fd->comm = tmp_comm;
	   MPI_Bcast(error_code, 1, MPI_INT, \
		     fd->hints->ranklist[0], fd->comm);
	   /* if no error, close the file and reopen normally below */
	   if (*error_code == MPI_SUCCESS)
	       (*(fd->fns->ADIOI_xxx_Close))(fd, error_code);

	   fd->access_mode = access_mode; /* back to original */
       }
       else MPI_Bcast(error_code, 1, MPI_INT, fd->hints->ranklist[0], fd->comm);

       if (*error_code != MPI_SUCCESS) {
	   if (fd->cache_fd && fd->cache_fd->is_open)
	       (*(fd->fns->ADIOI_xxx_Close))(fd->cache_fd, &max_error_code);
	   return;
       }
       else {
           /* turn off CREAT (and EXCL if set) for real multi-processor open */
           access_mode ^= ADIO_CREATE;
	   if (access_mode & ADIO_EXCL)
		   access_mode ^= ADIO_EXCL;
       }
    }

    /* if we are doing deferred open, non-aggregators should return now */
    if (fd->hints->deferred_open ) {
        if (fd->agg_comm == MPI_COMM_NULL) {
            /* we might have turned off EXCL for the aggregators.
             * restore access_mode that non-aggregators get the right
             * value from get_amode */
            fd->access_mode = orig_amode_excl;
            *error_code = MPI_SUCCESS;
	    return;
        }
    }

/* For writing with data sieving, a read-modify-write is needed. If
   the file is opened for write_only, the read will fail. Therefore,
   if write_only, open the file as read_write, but record it as write_only
   in fd, so that get_amode returns the right answer. */

    /* observation from David Knaak: file systems that do not support data
     * sieving do not need to change the mode */

    orig_amode_wronly = access_mode;
    if ( (access_mode & ADIO_WRONLY) &&
	    ADIO_Feature(fd, ADIO_DATA_SIEVING_WRITES) ) {
	access_mode = access_mode ^ ADIO_WRONLY;
	access_mode = access_mode | ADIO_RDWR;
    }
    fd->access_mode = access_mode;

    (*(fd->fns->ADIOI_xxx_Open))(fd, error_code);

    /* if error, may be it was due to the change in amode above.
       therefore, reopen with access mode provided by the user.*/
    fd->access_mode = orig_amode_wronly;
    if (*error_code != MPI_SUCCESS)
        (*(fd->fns->ADIOI_xxx_Open))(fd, error_code);

    /* if we turned off EXCL earlier, then we should turn it back on */
    if (fd->access_mode != orig_amode_excl) fd->access_mode = orig_amode_excl;

    /* for deferred open: this process has opened the file (because if we are
     * not an aggregaor and we are doing deferred open, we returned earlier)*/
    fd->is_open = 1;
}

/*
 * vim: ts=8 sts=4 sw=4 noexpandtab
 */
