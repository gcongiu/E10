/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 *   Copyright (C) 2014-2016 Seagate Systems UK Ltd.
 *   See COPYRIGHT notice in top-level directory.
 *
 *   Author: Giuseppe Congiu <giuseppe.congiu@seagate.com>
 */

#include "adio.h"
#include "ad_beegfs.h"
#include "adio_extern.h"
#include "adio_cb_config_list.h"

/*
 * ADIOI_BEEGFS_Mkdir - creates recursively all the directories in name
 *
 * This subroutine takes a pessimistic approach and tries 
 * to create all the directories in the pathname name using
 * deeper_cache_mkdir()
 */
static int ADIOI_BEEGFS_Mkdir(const char *name, mode_t perm) {
    int i = 1, len, ret = DEEPER_RETVAL_SUCCESS;

    if (name == NULL) {
	errno = ENOENT;
	return DEEPER_RETVAL_ERROR;
    }

    char *string = (char *)ADIOI_Malloc(strlen(name)+1);
    char *ptr = string;
    len = strlen(name);

    /* accounts for first '/' */
    while (i < len && (!ret || errno == EEXIST)) {
	*(ptr++) = '/';
	
	/* add path components to string */
	while (name[i] != '\0' && name[i] != '/')
	    *(ptr++) = name[i++];

	*ptr = '\0';

	/* create the nth dir in the pathname */
	if (deeper_cache_mkdir(string, perm)) {
	    FPRINTF(stderr, "%s error: %s\n", string, strerror(errno));
	}
	
	/* skip all the '/' in name */
	while (name[i] == '/')
	    i++;
    }

    ADIOI_Free(string);

    return DEEPER_RETVAL_SUCCESS;    
}

/* BeeGFS version of a "collective open" */
void ADIOI_BEEGFS_OpenColl(ADIO_File fd,
			   int rank,
			   int access_mode,
			   int *error_code)
{
    int orig_amode_excl, orig_amode_wronly, max_error_code;
    int ret = DEEPER_RETVAL_SUCCESS;
    char *pathname = NULL, *dir = NULL;
    char myname[] = "ADIOI_BEEGFS_OPENCOLL";
    MPI_Comm tmp_comm;

    orig_amode_excl = access_mode;

    /* Revert to absolute pathnames */
    pathname = ADIOI_Abspath(fd->filename);
    ADIOI_Free(fd->filename);
    fd->filename = pathname;

    /* Workout the file's parent directory name */
    dir = ADIOI_Dirname(fd->filename);

fn_open:
    if (access_mode & ADIO_CREATE) {
	if (rank == fd->hints->ranklist[0]) {
	    /* remove delete_on_close flag if set */
	    if (access_mode & ADIO_DELETE_ON_CLOSE)
                fd->access_mode = access_mode ^ ADIO_DELETE_ON_CLOSE;
            else
                fd->access_mode = access_mode;

	    /* Prefetch the file's parent dir in the e10_cache FS */
            if(fd->hints->e10_cache == ADIOI_HINT_ENABLE) {
		//ret = deeper_cache_mkdir(dir, S_IRWXU);
		ret = ADIOI_BEEGFS_Mkdir(dir, S_IRWXU);

                fd->cache_oflags  = DEEPER_OPEN_FLUSHONCLOSE;
		fd->cache_oflags |= DEEPER_OPEN_FLUSHWAIT;
	    } else {
		ret = DEEPER_RETVAL_SUCCESS;
	    }

	    /* set MPI_COMM_SELF for open operation */
            tmp_comm = fd->comm;
            fd->comm = MPI_COMM_SELF;

	    if (ret == DEEPER_RETVAL_SUCCESS ||
		(ret == DEEPER_RETVAL_ERROR && errno == EEXIST)) {
                /* only the root process creates the file using
		 * deeper_cache_open() call ... */
		(*(fd->fns->ADIOI_xxx_Open))(fd, error_code);
            } else if (ret == DEEPER_RETVAL_ERROR && errno != EEXIST) {
                /* -BEGIN ERROR HANDLING- */
                FPRINTF(stderr, "[rank:%d]Error while creating directory %s: %s\n",
			rank,
			dir,
                        strerror(errno));

                /* TODO: evaluate errno and generate appropriate romio error code */
                *error_code = MPIO_Err_create_code(MPI_SUCCESS,
                                                   MPIR_ERR_RECOVERABLE, myname,
                                                   __LINE__, MPI_ERR_IO,
                                                   "deeper_cache_prefetch %s",
                                                   strerror(errno));
                /* -END ERROR HANDLING- */
            }

	    /* restore mpi communicator */
	    fd->comm = tmp_comm;

	    /* if no error, close the file and reopen normally below */
	    if(*error_code == MPI_SUCCESS || *error_code == MPI_ERR_FILE_EXISTS) {
		/* close file and flush it to global file system */
		(*(fd->fns->ADIOI_xxx_Close))(fd, error_code);
	    }

	    /* broadcast open result */
	    MPI_Bcast(error_code, 1, MPI_INT, fd->hints->ranklist[0], fd->comm);

	    fd->access_mode = access_mode; /* back to original */
	}
	else MPI_Bcast(error_code, 1, MPI_INT, fd->hints->ranklist[0], fd->comm);

	if (*error_code != MPI_SUCCESS) {
	    if (fd->hints->e10_cache == ADIOI_HINT_ENABLE) {
		FPRINTF(stderr, "[rank:%d]Error while opening cache file, reverting \ 
			to standard implementation.\n", rank);
		ADIOI_Info_set(fd->info, "e10_cache", "disable");
		fd->hints->e10_cache = ADIOI_HINT_DISABLE;
		goto fn_open; /* retry using POSIX open */
	    }
	    else
		goto fn_exit; /* exit */
	}
	else {
	    /* turn off CREAT (and EXCL if set) for real multi-processor open */
	    access_mode ^= ADIO_CREATE;
	    if (access_mode & ADIO_EXCL)
		access_mode ^= ADIO_EXCL;
	}
    }
    fd->blksize = 1024*1024*4; /* this large default value should be good for
				 most file systems.  any ROMIO driver is free
				 to stat the file and find an optimial value */

    if (fd->hints->e10_cache == ADIOI_HINT_ENABLE) {
        /* allocate space for sync thread data structure */
        fd->thread_pool = (ADIOI_Sync_thread_t *)ADIOI_Malloc(sizeof(ADIOI_Sync_thread_t));
    
    	/* init sync thread data structure */ 
        ADIOI_BEEGFS_Sync_thread_init(&fd->thread_pool[0], fd);
    }

    /* if we are doing deferred open, non-aggregators should return now */
    if (fd->hints->deferred_open) {
        if (!(fd->is_agg)) {
            /* we might have turned off EXCL for the aggregators.
             * restore access_mode that non-aggregators get the right
             * value from get_amode */
            fd->access_mode = orig_amode_excl;
	    /* In file-system specific open, a driver might collect some
	     * information via stat().  Deferred open means not every process
	     * participates in fs-specific open, but they all participate in
	     * this open call.  Broadcast a bit of information in case
	     * lower-level file system driver (e.g. 'bluegene') collected it
	     * (not all do)*/
	    MPI_Bcast(&(fd->blksize), 1, MPI_LONG, fd->hints->ranklist[0], fd->comm);
	    *error_code = MPI_SUCCESS;
	    ADIOI_Assert(fd->blksize > 0);
	    goto fn_exit;
	}
    }

/* For writing with data sieving, a read-modify-write is needed. If
   the file is opened for write_only, the read will fail. Therefore,
   if write_only, open the file as read_write, but record it as write_only
   in fd, so that get_amode returns the right answer. */

    /* observation from David Knaak: file systems that do not support data
     * sieving do not need to change the mode */

    orig_amode_wronly = access_mode;
    if ((access_mode & ADIO_WRONLY) && ADIO_Feature(fd, ADIO_DATA_SIEVING_WRITES)) {
	access_mode = access_mode ^ ADIO_WRONLY;
	access_mode = access_mode | ADIO_RDWR;
    }
    fd->access_mode = access_mode;

    /* Now everyone opens the file created before */
    if (fd->hints->e10_cache == ADIOI_HINT_ENABLE) {
        /* create directory in the cache file system */
	//ret = deeper_cache_mkdir(dir, S_IRWXU);
	ret = ADIOI_BEEGFS_Mkdir(dir, S_IRWXU);

	/* check mkdir return val */
        if (ret == DEEPER_RETVAL_ERROR && errno != EEXIST) {
            FPRINTF(stderr, "[rank:%d]%s error: %s\n",
		    rank,
                    dir,
                    strerror(errno));
	}

	/* multiple calls on the same node will return EALREADY */
	ret  = deeper_cache_prefetch(fd->filename, DEEPER_PREFETCH_NONE);

	/* check prefetch return val. Please note that nothing should go wrong here */
	if (ret == DEEPER_RETVAL_ERROR && errno != EEXIST && errno != EALREADY) {
	    FPRINTF(stderr, "[rank:%d]%s error: %s\n",
		    rank,
		    dir,
		    strerror(errno));
	}

	/* wait here for prefetch to finish */
	ret = deeper_cache_prefetch_wait(fd->filename, DEEPER_PREFETCH_NONE);

        /* set the cache open flags for the file in the cache FS */
	fd->cache_oflags = DEEPER_OPEN_NONE;

        if (fd->hints->e10_cache_discard_flag == ADIOI_HINT_ENABLE)
            fd->cache_oflags |= DEEPER_OPEN_DISCARD;
    }

    /* open file in every process */
    (*(fd->fns->ADIOI_xxx_Open))(fd, error_code);

    /* if error, may be it was due to the change in amode above.
       therefore, reopen with access mode provided by the user.*/
    fd->access_mode = orig_amode_wronly;
    if (*error_code != MPI_SUCCESS)
        (*(fd->fns->ADIOI_xxx_Open))(fd, error_code);

    /* if we turned off EXCL earlier, then we should turn it back on */
    if (fd->access_mode != orig_amode_excl)
        fd->access_mode = orig_amode_excl;

    /* broadcast a bit of information (blocksize for now) to all proceses in
     * communicator, not just those who participated in open */
    MPI_Bcast(&(fd->blksize), 1, MPI_LONG, fd->hints->ranklist[0], fd->comm);
    /* file domain code will get terribly confused in a hard-to-debug way if
     * gpfs blocksize not sensible */
    ADIOI_Assert( fd->blksize > 0);
    /* for deferred open: this process has opened the file (because if we are
     * not an aggregaor and we are doing deferred open, we returned earlier)*/
    fd->is_open = 1;

 fn_exit:
    ADIOI_Free(dir);
}

/*
 * vim: ts=8 sts=4 sw=4 noexpandtab
 */
