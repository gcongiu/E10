/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 *   Copyright (C) 2014-2016 Seagate System UK Ltd
 *   See COPYRIGHT notice in top-level directory.
 *
 *   Author: Giuseppe Congiu <giuseppe.congiu@seagate.com>
 */

#include "ad_beegfs.h"
#include "adio_extern.h"
#include "hint_fns.h"
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

void ADIOI_BEEGFS_SetInfo( ADIO_File fd, MPI_Info users_info, int *error_code )
{
    char *value, *pathname, *dname, *slash;
    int flag, stripe_val[2], numtargets = 0, chunksize = 0;
    struct BeegfsIoctl_MkFileWithStripeHints_Arg createFileArg;
    int err, myrank, fd_pdir, perm, old_mask;
    static char myname[] = "ADIOI_BEEGFS_SETINFO";

    /* set error code to success */
    *error_code = MPI_SUCCESS;

    value = ( char * )ADIOI_Malloc( ( MPI_MAX_INFO_VAL + 1 ) * sizeof( char ) );

    MPI_Comm_rank( fd->comm, &myrank );

    /* set hints */
    if( ( fd->info ) == MPI_INFO_NULL ) {
	MPI_Info_create( &( fd->info ) );

	ADIOI_Info_set( fd->info, "striping_unit", "0" );
	ADIOI_Info_set( fd->info, "striping_factor", "0" );

	/* set users infos */
	if( users_info != MPI_INFO_NULL ) {
	    /* striping information */
	    ADIOI_Info_get( users_info, "striping_unit", MPI_MAX_INFO_VAL, value, &flag );
	    if( flag )
		chunksize = atoi( value );

	    ADIOI_Info_get( users_info, "striping_factor", MPI_MAX_INFO_VAL, value, &flag );
	    if( flag )
		numtargets = atoi( value );

	    /* check stripe info consistency */
	    if( myrank == 0 ) {
		stripe_val[0] = numtargets;
		stripe_val[1] = chunksize;
	    }
	    MPI_Bcast( stripe_val, 2, MPI_INT, 0, fd->comm );

	    if( stripe_val[0] != numtargets || stripe_val[1] != chunksize ) {
		FPRINTF( stderr, "ADIOI_BEEGFS_SetInfo: All keys"
			         "-striping_factor:striping_unit "
			         "need to be identical across all processes\n" );
		MPI_Abort( MPI_COMM_WORLD, 1 );
	    }

	    /* if user has specified striping info, process 0 tries to set it */
	    if( myrank == 0 && ( fd->access_mode & ADIO_CREATE ) && numtargets && chunksize ) {
		/* open the parent dir to get/set striping info */
		pathname = ADIOI_Strdup( fd->filename );
		dname = strrchr( pathname, '/' );
		if( dname != NULL ) {
		    *dname = '\0'; // replace / with nul-character
		    fd_pdir = open( pathname, O_RDONLY );
		    if( fd_pdir == -1 ) {
			FPRINTF( stderr, "Error opening %s: %s\n", pathname, strerror( errno ) );
		    }
		}
		else {
		    /* current dir relative path */
		    fd_pdir = open( ".", O_RDONLY );
		    if( fd_pdir == -1 ) {
			FPRINTF( stderr, "Error opening .: %s\n", strerror( errno ) );
		    }
		}
		ADIOI_Free( pathname );

		if( fd->perm == ADIO_PERM_NULL ) {
		    old_mask = umask( 022 );
		    umask( old_mask );
		    perm = old_mask ^ 0666;
		}
		else perm = fd->perm;

		/* set create hints depending on e10 hints previously set */
		slash = strrchr( fd->filename, '/' );
		if( slash != NULL )
		    slash += 1;
		else
		    slash = fd->filename;

		createFileArg.filename = slash;
		createFileArg.mode = perm;
		createFileArg.numtargets = numtargets;
		createFileArg.chunksize = chunksize;

		/* create the hint file */
		err = ioctl( fd_pdir, BEEGFS_IOC_MKFILE_STRIPEHINTS, &createFileArg );
		if( err ) {
		    FPRINTF( stderr, "BEEGFS_IOC_MKFILE_STRIPEHINTS: %s. ", strerror( errno ) );
		    if( errno == EEXIST ) {
			/* ignore user striping and use current file info */
			FPRINTF( stderr, "[rank:%d] Failure to set stripe info for %s!\n", myrank, fd->filename );
		    }
		}
		/* close the parent dir file descriptor */
		close( fd_pdir );
	    } /* End of striping parameters validation */
	}

	MPI_Barrier( fd->comm );
    }

    /* set rest of the MPI hints (including E10 hints) */
    ADIOI_GEN_SetInfo( fd, users_info, error_code );

    ADIOI_Free( value );
}

/*
 * vim: ts=8 sts=4 sw=4 noexpandtab
 */
