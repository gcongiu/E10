/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 *   Copyright (C) 2014-2016 Seagate Systems UK Ltd.
 *   See COPYRIGHT notice in top-level directory.
 *
 *   Author: Giuseppe Congiu <giuseppe.congiu@seagate.com>
 */

#include "adio.h"
#include "ad_beegfs.h"

/* BeeGFS version of a "collective open" */
void ADIOI_BEEGFS_OpenColl(ADIO_File fd,
			   int rank,
			   int access_mode,
			   int *error_code)
{
    int orig_amode_excl, orig_amode_wronly, max_error_code, pflags;
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

	    /* set MPI_COMM_SELF for open operation */
            tmp_comm = fd->comm;
            fd->comm = MPI_COMM_SELF;

	    /* Prefetch the file's parent dir in the e10_cache FS */
            if(fd->hints->e10_cache == ADIOI_HINT_ENABLE) {
                pflags = DEEPER_PREFETCH_SUBDIRS | DEEPER_PREFETCH_WAIT;

                ret = deeper_cache_prefetch(dir, pflags);

                /* Flush the file in the e10_cache FS to the global FS */
                fd->cache_oflags = DEEPER_OPEN_FLUSHONCLOSE |
                                       DEEPER_OPEN_FLUSHWAIT;
	    }

	    if (ret == DEEPER_RETVAL_SUCCESS ||
		(ret == DEEPER_RETVAL_ERROR && errno == EEXIST)) {
                /* only the root process creates the file using
		 * deeper_cache_open() call ... */
		(*(fd->fns->ADIOI_xxx_Open))(fd, error_code);
            }
            else {
                /* -BEGIN ERROR HANDLING- */
                FPRINTF(stderr, "[rank:%d]Error while prefetching %s: %s\n",
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
        }

	/* restore mpi communicator */
        fd->comm = tmp_comm;

	/* broadcast open result */
	MPI_Bcast(error_code, 1, MPI_INT, fd->hints->ranklist[0], fd->comm);

        /* if no error, close the file and reopen normally below */
        if(*error_code == MPI_SUCCESS)
            (*(fd->fns->ADIOI_xxx_Close))(fd, error_code);

        fd->access_mode = access_mode; /* back to original */
    }
    else
        MPI_Bcast(error_code, 1, MPI_INT, fd->hints->ranklist[0], fd->comm);

    if (*error_code != MPI_SUCCESS) {
        if (fd->hints->e10_cache == ADIOI_HINT_ENABLE) {
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

    /* if we are doing deferred open, non-aggregators should return now */
    if (fd->hints->deferred_open) {
        if (fd->agg_comm == MPI_COMM_NULL) {
            /* we might have turned off EXCL for the aggregators.
             * restore access_mode that non-aggregators get the right
             * value from get_amode */
            fd->access_mode = orig_amode_excl;
            *error_code = MPI_SUCCESS;
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
        /* Prefetch the file in the cache FS */
        ret = deeper_cache_prefetch(fd->filename, DEEPER_PREFETCH_WAIT);

        if (ret == DEEPER_RETVAL_ERROR)
            FPRINTF(stderr, "Error while prefetching %s: %s\n",
                    fd->filename,
                    strerror(errno));

        /* set the cache open flags for the file in the cache FS */
        fd->cache_oflags = DEEPER_OPEN_NONE;
        if (fd->hints->e10_cache_flush_flag == ADIOI_HINT_FLUSHONCLOSE) {
            fd->cache_oflags  = DEEPER_OPEN_FLUSHONCLOSE;
            fd->cache_oflags |= DEEPER_OPEN_FLUSHWAIT;
        }

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

    /* for deferred open: this process has opened the file (because if we are
     * not an aggregaor and we are doing deferred open, we returned earlier)*/
    fd->is_open = 1;

 fn_exit:
    ADIOI_Free(dir);
}

/*
 * vim: ts=8 sts=4 sw=4 noexpandtab
 */