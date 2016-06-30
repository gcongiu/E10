/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 *   Copyright (C) 2014-2016 Seagate Systems UK Ltd.
 *   See COPYRIGHT notice in top-level directory.
 *
 *   Author: Giuseppe Congiu <giuseppe.congiu@seagate.com>
 */

#include "ad_beegfs.h"
#include "adio_extern.h"

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

void ADIOI_BEEGFS_Close( ADIO_File fd, int *error_code )
{
    int err, derr = 0;
    static char myname[] = "ADIOI_BEEGFS_CLOSE";

#ifdef ADIOI_MPE_LOGGING
    MPE_Log_event( ADIOI_MPE_close_a, 0, NULL );
#endif

#ifndef BEEGFS_UFS_CACHE_TEST
    err = (fd->hints->e10_cache == ADIOI_HINT_ENABLE) ?
	    deeper_cache_close( fd->fd_sys ) :
	    close( fd->fd_sys );

    ADIOI_BEEGFS_Sync_thread_fini( fd->thread_pool );
#else
    close( fd->fd_sys );
#endif

#ifdef ADIOI_MPE_LOGGING
    MPE_Log_event( ADIOI_MPE_close_b, 0, NULL );
#endif

    fd->fd_sys    = -1;
    fd->fd_direct = -1;

    if( err == -1 || derr == -1 ) {
	*error_code = ADIOI_Err_create_code( myname, fd->filename, errno );
    }
    else *error_code = MPI_SUCCESS;

    fd->is_open = 0;
}

/*
 * vim: ts=8 sts=4 sw=4 noexpandtab
 */