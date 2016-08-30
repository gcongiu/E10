/* -*- Mode: C; c-basic-offset:4 ; -*- */
/*
 *  (C) 2001 by Argonne National Laboratory.
 *      See COPYRIGHT in top-level directory.
 *
 *  (C) 2014-2016 Seagate Systems UK Ltd.
 *
 *  Author: Giuseppe Congiu <giuseppe.congiu@seagate.com>
 *
 *  Name: coll_perf.c
 */

/* Description: modification of the coll_perf benchmark from Argonne.
 *              Now a number of different shared file can be written
 *              in sequence by specifying the number of CYCLES in the
 *              '-i' parameter. Processing is emulated using a matrix
 *              multiplication kernel. Amount of data written by every
 *              process can also be specified using the '-b' option.
 */
#include <stdio.h>
#include <unistd.h>
#include <stdarg.h>
#include <fcntl.h>
#include <sys/param.h>
#include <stdlib.h>
#include <errno.h>
#include <assert.h>
#include <stdint.h>
#include <math.h>
#include <signal.h>
#include <mpi.h>
#include <string.h>
#include "matrix_mult.h"

#define N 1300

void term_handler( int sig )
{
	/* Restore the default SIGABRT disposition */
	signal(SIGABRT, SIG_DFL);
	/* Abort (dumps core) */
	abort();
}

/* print usage */
void print_usage( char *prog_name )
{
	fprintf( stderr, "Usage: %s [options]\n\n", prog_name );
	fprintf( stderr, "options:\n" );
	fprintf( stderr, "  -f NAME    basename of the output file [NAME_CYCLE]\n" );
	fprintf( stderr, "  -d DELAY   simulated delay between iterations\n" );
	fprintf( stderr, "  -i CYCLES  iterations of processing + write\n" );
	fprintf( stderr, "  -b BSIZE   block size, (BSIZE)^3 x 4Bytes = MB/proc\n" );
	fprintf( stderr, "             default BSIZE is 256 -> 64MB/proc\n" );
	fprintf( stderr, "  -h         print this message and exit\n" );
}

int main( int argc, char **argv )
{
	MPI_Datatype newtype;
	int i, ndims, array_of_gsizes[3], array_of_distribs[3];
	int order, nprocs, len, *buf, bufcount, mynod;
	int array_of_dargs[3], array_of_psizes[3], blksize = 256;
	MPI_File *fh;
	MPI_Status *status;
	MPI_Info info;
	double stim, write_tim, new_write_tim, write_bw; //write time
	double read_tim, new_read_tim, read_bw;          //read time
	double sgtim, gtim, new_gtim, dtim, new_dtim;    //global time
        double delay = 0;
	char *filename = NULL;
	int opt, ret, amode, iterations = 1;
	int **A, **B, **C;
	char *optarg, curr_filename[128];

	/* allocate matrix */
	A = ( int** )malloc( sizeof( int* )*N );
	B = ( int** )malloc( sizeof( int* )*N );
	C = ( int** )malloc( sizeof( int* )*N );
	for( i = 0; i < N; i++ )
	{
		A[i] = ( int* )malloc( sizeof( int )*N );
		B[i] = ( int* )malloc( sizeof( int )*N );
		C[i] = ( int* )malloc( sizeof( int )*N );
	}

	/* initialize MPI */
	MPI_Init( &argc, &argv );
	MPI_Comm_rank( MPI_COMM_WORLD, &mynod );
	MPI_Comm_size( MPI_COMM_WORLD, &nprocs );

#ifdef ENABLE_DEBUG
	if( mynod == 0 )
	{
		i = 0;
		fprintf( stdout, "PID: %d\n", getpid( ) );
		while( i == 0 )
			sleep( 5 );
	}
#endif
        signal( SIGTERM, term_handler );

#ifdef OPTARG /* getopt is not working with mpiexec */
	/* scan for inputs */
	while( mynod == 0 && (opt = getopt( argc, argv, "f:d:i:b:h" )) != -1 )
	{
		switch( opt )
		{
			case 'f':
				filename = strdup( optarg );
				break;
			case 'd':
				delay = ( double )atof( optarg );
				delay *= 1000000; /* convert to us */
				break;
			case 'i':
				iterations = atoi( optarg );
				break;
			case 'b':
				blksize = atoi( optarg );
				break;
			case 'h':
				print_usage( argv[0] );
				exit( EXIT_SUCCESS );
			default:
				print_usage( argv[0] );
				exit( EXIT_FAILURE );
		}
	}
#else
	i = 1;
	while( mynod == 0 && i < argc )
	{
		if( !strcmp( "-f", *argv ) )
		{
			argv++;
			i++;
			filename = strdup( *argv );
		}
		else if( !strcmp( "-d", *argv ) )
		{
			argv++;
			i++;
			delay = ( double )atof( *argv );
			delay *= ( double )1000000;
		}
		else if( !strcmp( "-i", *argv ) )
		{
			argv++;
			i++;
			iterations = atoi( *argv );
		}
		else if( !strcmp( "-b", *argv ) )
		{
			argv++;
			i++;
			blksize = atoi( *argv );
		}
		else if( !strcmp( "-h", *argv ) )
		{
			print_usage( argv[0] );
			exit( EXIT_SUCCESS );
		}
		i++;
		argv++;
	}
#endif
	/* if filename is not defined */
	if( mynod == 0 && filename == NULL )
	{
		print_usage( argv[0] );
		exit( EXIT_FAILURE );
	}

	/* print simulation parameters */
	if( mynod == 0 )
	{
		fprintf( stdout, "#\n");
		fprintf( stdout, "# Simulation Parameters:\n");
		fprintf( stdout, "#   BASENAME: %s\n", filename );
		fprintf( stdout, "#   DELAY:    %f\n", delay );
		fprintf( stdout, "#   CYCLES:   %d\n", iterations );
		fprintf( stdout, "#   NPROCS:   %d\n", nprocs );
		fprintf( stdout, "#   Nx,Ny,Nz: %d\n", blksize );
		fprintf( stdout, "#\n" );

		/* broadcast parameters */
		len = strlen( filename );
		MPI_Bcast( &len, 1, MPI_INT, 0, MPI_COMM_WORLD );
		MPI_Bcast( filename, len+1, MPI_CHAR, 0, MPI_COMM_WORLD );
		MPI_Bcast( &delay, 1, MPI_DOUBLE, 0, MPI_COMM_WORLD );
		MPI_Bcast( &iterations, 1, MPI_INT, 0, MPI_COMM_WORLD );
		MPI_Bcast( &blksize, 1, MPI_INT, 0, MPI_COMM_WORLD );
	}
	else
	{
		/* receive input parameters from proc 0 */
		MPI_Bcast( &len, 1, MPI_INT, 0, MPI_COMM_WORLD );
		filename = ( char * )malloc( len+1 );
		MPI_Bcast( filename, len+1, MPI_CHAR, 0, MPI_COMM_WORLD );
		MPI_Bcast( &delay, 1, MPI_DOUBLE, 0, MPI_COMM_WORLD );
		MPI_Bcast( &iterations, 1, MPI_INT, 0, MPI_COMM_WORLD );
		MPI_Bcast( &blksize, 1, MPI_INT, 0, MPI_COMM_WORLD );
	}
#ifdef ENABLE_DEBUG
	MPI_Barrier( MPI_COMM_WORLD );
#endif
	// NOTE: the user must specify at least 8 procs (nx=2, ny=2, nz=2)
	if( nprocs < 8 )
		exit( EXIT_FAILURE );

	ndims = 3;
	order = MPI_ORDER_C;

	array_of_distribs[0] = MPI_DISTRIBUTE_BLOCK;
	array_of_distribs[1] = MPI_DISTRIBUTE_BLOCK;
	array_of_distribs[2] = MPI_DISTRIBUTE_BLOCK;

	array_of_dargs[0] = MPI_DISTRIBUTE_DFLT_DARG;
	array_of_dargs[1] = MPI_DISTRIBUTE_DFLT_DARG;
	array_of_dargs[2] = MPI_DISTRIBUTE_DFLT_DARG;

	for( i = 0; i < ndims; i++ )
		array_of_psizes[i] = 0;

	MPI_Dims_create( nprocs, ndims, array_of_psizes );

	/**
	 * array_of_gsizes = {
	 *                     128 -> 8MB/proc
	 *                     256 -> 64MB/proc
	 *                     512 -> 512MB/proc
	 *                   }
	 */
	array_of_gsizes[0] = blksize * array_of_psizes[0];
	array_of_gsizes[1] = blksize * array_of_psizes[1];
	array_of_gsizes[2] = blksize * array_of_psizes[2];

	MPI_Type_create_darray( nprocs, mynod, ndims, array_of_gsizes,
			        array_of_distribs, array_of_dargs,
			        array_of_psizes, order, MPI_INT, &newtype );

	MPI_Type_commit( &newtype );

	MPI_Type_size( newtype, &bufcount );
	bufcount = bufcount / sizeof( int );
	buf = ( int * )malloc( bufcount * sizeof( int ) );

	/* assign my rank to every element of buf */
	for( i = 0; i < bufcount; i++ )
		buf[i] = mynod;

	fh = ( MPI_File * )malloc( sizeof( MPI_File )*iterations );
	status = ( MPI_Status * )malloc( sizeof( MPI_Status ) );
	info = MPI_INFO_NULL;

	i = 0;
	write_tim = 0;
	amode = MPI_MODE_CREATE | MPI_MODE_RDWR;
	sgtim = MPI_Wtime( );
	while( i < iterations )
	{
		/* write next file */
		sprintf( curr_filename, "%s_%d\0", filename, i );
		stim = MPI_Wtime( );
		if( (ret = MPI_File_open( MPI_COMM_WORLD, curr_filename, amode, info, fh+i ) ) != MPI_SUCCESS )
		{
			fprintf( stderr, "MPI_File_open of %s failed with error %d\n", curr_filename, ret );
			MPI_Abort( MPI_COMM_WORLD, ret );
		}
		write_tim += MPI_Wtime( ) - stim;

		MPI_File_set_view( fh[i], 0, MPI_INT, newtype, "native", info );

		stim = MPI_Wtime( );
		if ( (ret = MPI_File_write_all( fh[i], buf, bufcount, MPI_INT, status )) != MPI_SUCCESS )
        	{
			fprintf( stderr, "MPI_File_write_all failed writing to %s with error %d\n",
				 curr_filename, ret );
			MPI_Abort( MPI_COMM_WORLD, ret );
		}
		write_tim += MPI_Wtime( ) - stim;

		/* close file */
		stim = MPI_Wtime( );
		MPI_File_close( fh+i );
		write_tim += MPI_Wtime( ) - stim;

                /* wait for all the processes */
                MPI_Barrier( MPI_COMM_WORLD );

                /* emulates computing */
                stim = MPI_Wtime( );
		usleep( delay );
		//( void )matrix_mult( A, B, N, C );
		dtim += MPI_Wtime( ) - stim;

		i += 1;
	}

	gtim = MPI_Wtime( ) - sgtim; // stop global timer

	/* synchronize and exchange time */
	MPI_Allreduce( &write_tim, &new_write_tim, 1, MPI_DOUBLE, MPI_MAX, MPI_COMM_WORLD );
	MPI_Allreduce( &gtim, &new_gtim, 1, MPI_DOUBLE, MPI_MAX, MPI_COMM_WORLD );
	MPI_Allreduce( &dtim, &new_dtim, 1, MPI_DOUBLE, MPI_MAX, MPI_COMM_WORLD );

	if( mynod == 0 )
	{
		size_t size = array_of_gsizes[0] * array_of_gsizes[1] * array_of_gsizes[2];
		size *= sizeof( int ) * iterations;
		size /= 1024 * 1024;
		write_bw = ( double )( size / new_write_tim );
		fprintf( stdout, "Array of psize = %d x %d x %d integers\n", array_of_psizes[0], array_of_psizes[1], array_of_psizes[2] );
		fprintf( stdout, "Global array size = %d x %d x %d integers\n", array_of_gsizes[0], array_of_gsizes[1], array_of_gsizes[2] );
		fprintf( stdout, "Total written data = %llu MB\n", size );
		fprintf( stdout, "Collective write time = %f sec\n", new_write_tim );
		fprintf( stdout, "Collective write bandwidth = %f Mbytes/sec\n", write_bw );
		fprintf( stdout, "Global benchmark time = %f sec\n", new_gtim );
		fprintf( stdout, "Total compute time = %f sec\n", new_dtim );
	}

	MPI_Barrier( MPI_COMM_WORLD );

	MPI_Type_free( &newtype );
	for( i = 0; i < N; i++ )
	{
		free( A[i] );
		free( B[i] );
		free( C[i] );
	}
	free( A );
	free( B );
	free( C );
	free( buf );
	free( filename );
	free( fh );
	free( status );


	MPI_Finalize( );
	return 0;
}
