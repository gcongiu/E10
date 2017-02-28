/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 *   Copyright (C) 2014-2016 Seagate Systems UK Ltd.
 *   See COPYRIGHT notice in top-level directory.
 *
 *   Author: Giuseppe Congiu <giuseppe.congiu@seagate.com>
 */

#include "adio.h"

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

void ADIOI_BEEGFS_Flush(ADIO_File fd, int *error_code)
{
    int err, myrank;
    static char myname[] = "ADIOI_BEEGFS_FLUSH";

    MPI_Comm_rank(fd->comm, &myrank);

    *error_code = MPI_SUCCESS;

    if (fd->hints->e10_cache_flush_flag == ADIOI_HINT_FLUSHNONE ||
	    !fd->thread_pool)
	goto fn_flush;

    /* Flush all the requests in each thread */
    ADIOI_BEEGFS_Sync_thread_flush(*(fd->thread_pool));

    /* Wait for submitted requests to complete */
    ADIOI_BEEGFS_Sync_thread_wait(*(fd->thread_pool));

    return;

fn_flush:
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

    *error_code = MPI_SUCCESS;
}

/*
 * vim: ts=8 sts=4 sw=4 noexpandtab
 */
