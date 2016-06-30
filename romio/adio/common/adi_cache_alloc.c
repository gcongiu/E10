/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 *
 *   Copyright (C) 2014-2016 Seagate Systems UK Ltd.
 *   See COPYRIGHT notice in top-level directory.
 *
 *   Author: Giuseppe Congiu <giuseppe.congiu@seagate.com>
 */

#include "adio.h"

/*
 * ADIOI_Cache_alloc - allocate space in the local file system for the cache file
 */
void ADIOI_Cache_alloc(ADIO_File fd, ADIO_Offset off, ADIO_Offset len, int *error_code)
{
    int ret;
    char myname[] = "ADIOI_CACHE_ALLOC";
    *error_code = MPI_SUCCESS;

    ret = fallocate(fd->fd_sys, 0, (off_t)off, (off_t)len);

    if (ret == -1)
        if (errno == ENOSPC)
            *error_code = MPIO_Err_create_code(MPI_SUCCESS,
					       MPIR_ERR_RECOVERABLE,
                                               myname, __LINE__,
                                               MPI_ERR_NO_SPACE, "**filenospace",
                                               "**filenospace %s", strerror(errno));
        else if (errno == EBADF)
            *error_code = MPIO_Err_create_code(MPI_SUCCESS,
                                               MPIR_ERR_RECOVERABLE,
                                               myname, __LINE__,
                                               MPI_ERR_ACCESS, "**fileaccess",
                                               "**fileaccess %s", strerror(errno));
        else if (errno == EIO)
            *error_code = MPIO_Err_create_code(MPI_SUCCESS,
                                               MPIR_ERR_RECOVERABLE,
                                               myname, __LINE__,
                                               MPI_ERR_IO, "**io",
                                               "**io %s", strerror(errno));
	/* if the system call is not supported proceed as if
	 * the allocation was successfull. We do this since
	 * it would take much more time to write zeros to the
	 * file
	 */
        /* else if( errno == ENOSYS ) */
}

/*
 * vim: ts=8 sts=4 sw=4 noexpandtab
 */
