/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 *   Copyright (C) 2014-2016 Seagate Systems UK Ltd.
 *   See COPYRIGHT notice in top-level directory.
 *
 *   Author: Giuseppe Congiu <giuseppe.congiu@seagate.com>
 */

#include "ad_beegfs.h"
#include <unistd.h>

void ADIOI_BEEGFS_Open( ADIO_File fd, int *error_code )
{
    int perm, old_mask, amode, len;
    char *pathname, *value;
    static char myname[] = "ADIOI_BEEGFS_OPEN";

    if( fd->perm == ADIO_PERM_NULL ) {
	old_mask = umask( 022 );
	umask( old_mask );
	perm = old_mask ^ 0666;
    }
    else perm = fd->perm;

    amode = 0;
    if( fd->access_mode & ADIO_CREATE )
	amode = amode | O_CREAT;
    if( fd->access_mode & ADIO_RDONLY )
	amode = amode | O_RDONLY;
    if( fd->access_mode & ADIO_WRONLY )
	amode = amode | O_WRONLY;
    if( fd->access_mode & ADIO_RDWR )
	amode = amode | O_RDWR;
    if( fd->access_mode & ADIO_EXCL )
	amode = amode | O_EXCL;

#ifdef ADIOI_MPE_LOGGING
    MPE_Log_event( ADIOI_MPE_open_a, 0, NULL );
#endif

#ifndef BEEGFS_UFS_CACHE_TEST
    fd->fd_sys = (fd->hints->e10_cache == ADIOI_HINT_ENABLE) ?
	deeper_cache_open( fd->filename, amode, perm, fd->cache_oflags ) :
	open( fd->filename, amode, perm );

    if( fd->fd_sys != DEEPER_RETVAL_ERROR && fd->hints->e10_cache == ADIOI_HINT_ENABLE ) {
	fd->thread_pool = (ADIOI_Sync_thread_t *)ADIOI_Malloc(sizeof(ADIOI_Sync_thread_t));
	ADIOI_BEEGFS_Sync_thread_init( &fd->thread_pool[0], fd );
    }
#else
    fd->fd_sys = open( fd->filename, amode, perm );
#endif

#ifdef ADIOI_MPE_LOGGING
    MPE_Log_event( ADIOI_MPE_open_b, 0, NULL );
#endif
    fd->fd_direct = -1;

    if( fd->fd_sys != DEEPER_RETVAL_ERROR ) {
        int err;
        struct BeegfsIoctl_GetStripeInfo_Arg getStripeInfo;

        err = ioctl( fd->fd_sys, BEEGFS_IOC_GET_STRIPEINFO, &getStripeInfo );
	if( !err ) {
            value = ( char* )ADIOI_Malloc( (MPI_MAX_INFO_VAL + 1)*sizeof( char ) );
            fd->hints->striping_unit = getStripeInfo.outChunkSize;
            sprintf( value, "%d", getStripeInfo.outChunkSize );
            ADIOI_Info_set( fd->info, "striping_unit", value );

            fd->hints->striping_factor = getStripeInfo.outNumTargets;
            sprintf( value, "%d", getStripeInfo.outNumTargets );
            ADIOI_Info_set( fd->info, "striping_factor", value );

            ADIOI_Free( value );
        }
#ifdef ADIOI_MPE_LOGGING
        MPE_Log_event( ADIOI_MPE_lseek_a, 0, NULL );
#endif
        if( fd->access_mode & ADIO_APPEND )
            fd->fp_ind = fd->fp_sys_posn = lseek( fd->fd_sys, 0, SEEK_END );
#ifdef ADIOI_MPE_LOGGING
        MPE_Log_event( ADIOI_MPE_lseek_b, 0, NULL );
#endif
    }

    /* --BEGIN ERROR HANDLING-- */
    if( fd->fd_sys == DEEPER_RETVAL_ERROR ) {
	if( errno == ENAMETOOLONG )
	    *error_code = MPIO_Err_create_code( MPI_SUCCESS,
					        MPIR_ERR_RECOVERABLE, myname,
					        __LINE__, MPI_ERR_BAD_FILE,
					        "**filenamelong",
					        "**filenamelong %s %d",
					        fd->filename,
					        strlen( fd->filename ) );
	else if( errno == ENOENT )
	    *error_code = MPIO_Err_create_code( MPI_SUCCESS,
					        MPIR_ERR_RECOVERABLE, myname,
					        __LINE__, MPI_ERR_NO_SUCH_FILE,
					        "**filenoexist",
					        "**filenoexist %s",
					        fd->filename );
	else if( errno == ENOTDIR || errno == ELOOP )
	    *error_code = MPIO_Err_create_code( MPI_SUCCESS,
					        MPIR_ERR_RECOVERABLE,
					        myname, __LINE__,
					        MPI_ERR_BAD_FILE,
					        "**filenamedir",
					        "**filenamedir %s",
					        fd->filename );
	else if( errno == EACCES )
	    *error_code = MPIO_Err_create_code( MPI_SUCCESS,
					        MPIR_ERR_RECOVERABLE, myname,
					        __LINE__, MPI_ERR_ACCESS,
					        "**fileaccess",
					        "**fileaccess %s",
					        fd->filename );
	else if( errno == EROFS )
	    /* Read only file or file system and write access requested */
	    *error_code = MPIO_Err_create_code( MPI_SUCCESS,
					        MPIR_ERR_RECOVERABLE, myname,
					        __LINE__, MPI_ERR_READ_ONLY,
					        "**ioneedrd", 0 );
        else if( errno == EISDIR )
            *error_code = MPIO_Err_create_code( MPI_SUCCESS,
                                                MPIR_ERR_RECOVERABLE, myname,
                                                __LINE__, MPI_ERR_BAD_FILE,
                                                "**filename", 0 );
        else if( errno == EEXIST )
            *error_code = MPIO_Err_create_code( MPI_SUCCESS,
                                                MPIR_ERR_RECOVERABLE, myname,
                                                __LINE__, MPI_ERR_FILE_EXISTS,
                                                "**fileexist", 0 );
	else
	    *error_code = MPIO_Err_create_code( MPI_SUCCESS,
					        MPIR_ERR_RECOVERABLE, myname,
					        __LINE__, MPI_ERR_IO, "**io",
					        "**io %s", strerror( errno ) );
    }
    /* --END ERROR HANDLING-- */
    else *error_code = MPI_SUCCESS;
}

/*
 * vim: ts=8 sts=4 sw=4 noexpandtab
 */
