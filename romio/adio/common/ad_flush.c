/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/* 
 *
 *   Copyright (C) 1997 University of Chicago. 
 *   See COPYRIGHT notice in top-level directory.
 */

#include "adio.h"

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

void ADIOI_GEN_Flush(ADIO_File fd, int *error_code)
{
    int err, threads, idx;
    static char myname[] = "ADIOI_GEN_FLUSH";

    *error_code = MPI_SUCCESS;

    if (fd->cache_fd == NULL || fd->thread_pool == NULL ||
	(fd->cache_fd && !fd->cache_fd->is_open) ||
	(fd->cache_fd && fd->cache_fd->is_open &&
	 fd->hints->e10_cache_flush_flag == ADIOI_HINT_FLUSHNONE))
        goto fn_flush;

    threads = fd->hints->e10_cache_threads;
#ifdef ADIOI_MPE_LOGGING
    MPE_Log_event(ADIOI_MPE_thread_flush_a, 0, NULL);
#endif
    /* Flush all the requests in each thread */
    for (idx = 0; idx < threads; idx++) {
	ADIOI_Sync_thread_flush(fd->thread_pool[idx]);
    }
#ifdef ADIOI_MPE_LOGGING
    MPE_Log_event(ADIOI_MPE_thread_flush_b, 0, NULL);
    MPE_Log_event(ADIOI_MPE_thread_wait_a, 0, NULL);
#endif
    /* Wait for submitted requests to complete */
    for (idx = 0; idx < threads; idx++) {
	ADIOI_Sync_thread_wait(fd->thread_pool[idx]);
    }
#ifdef ADIOI_MPE_LOGGING
    MPE_Log_event(ADIOI_MPE_thread_wait_b, 0, NULL);
#endif
    return;

fn_flush:
   /* the deferred-open optimization may mean that a file has not been opened
     * on this processor */
    if (fd->is_open > 0) {
	err = fsync(fd->fd_sys);
	/* --BEGIN ERROR HANDLING-- */
	if (err == -1) {
	    *error_code = MPIO_Err_create_code(MPI_SUCCESS, MPIR_ERR_RECOVERABLE,
		    myname, __LINE__, MPI_ERR_IO,
		    "**io",
		    "**io %s", strerror(errno));
	    return;
	}
	/* --END ERROR HANDLING-- */
    }

    *error_code = MPI_SUCCESS;
}
