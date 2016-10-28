/*
 *  (C) 2014-2016 Seagate Systems UK Ltd.
 *      See COPYRIGHT in this directory.
 *
 *  Author: Giuseppe Congiu <giuseppe.congiu@seagate.com>
 *
 *  Name: mpiwrap.cpp
 *
 *  mpiwrap can work in three modes:
 *  1) without MPE support. In this mode mpiwrap will provide
 *     strong MPI_{Init,Finalize} and MPI_File_{open,close}
 *     symbols definitions to override the weak versions provided
 *     by libmpich.so
 *  2) with MPE support. In this mode mpiwrap would not be able
 *     to use the same mechanism as before (i.e. providing strong
 *     symbols definitions) normally. Indeed, liblmpe.a already
 *     provides strong symbols for all the MPI functions.
 *     The only way to make mpiwrap intercept the desidered
 *     functions calls is to modify the liblmpe.a object definining
 *     those symbols (i.e. log_mpi_core.o), renaming the
 *     symbols as __real_oldname (i.e. __real_MPI_Init, etc ...)
 *     This can be done by using the mpiwrap.sh script provided
 *     in this directory (type "bash mpiwrap.sh -h" for help).
 *  3) with DARSHAN support. In this mode mpiwrap will redirect
 *     mpi calls to DARSHAN using dlsym(). This should provide
 *     full compatibility with the JUBE environment.
 *
 *  TODO: Currently not considering the case in which the app
 *     tries to open a file to read it back. In this case we
 *     need to keep track of file names as well and make sure
 *     if a file is reopened, the previous file handle for
 *     it has been closed, and thus the cache is synchronized
 *     with global FS.
 */
extern "C"
{
        #include <stdio.h>
        #include <unistd.h>
        #include <libgen.h> // dirname
        #include <stdarg.h>
        #include <fcntl.h>
        #include <sys/param.h>
        #include <stdlib.h>
        #include <string.h>
        #include <errno.h>
        #include <assert.h>
#if defined(_WITH_DARSHAN_)
        /* use dlsym */
        #define dlsym __
        #include <dlfcn.h>
        #undef dlsym
#endif
        /* MPI-IO API */
        #define MPICH_SKIP_MPICXX
        #include <mpi.h>
};

#include <json/json.h>
#include <map>
#include <list>
#include <iostream>
#include <sstream>

/* Local Functions Prototypes (Use extern "C" to avoid g++ name mangling) */
static int ( *_MPI_Init )      ( int*, char*** )                                                      = NULL;
static int ( *_MPI_Init_thread)( int*, char***, int, int* )                                           = NULL;
static int ( *_MPI_Finalize )  ( void )                                                               = NULL;
static int ( *_MPI_File_open ) ( MPI_Comm, const char*, int, MPI_Info, MPI_File* )                    = NULL;
static int ( *_MPI_File_close )( MPI_File* )                                                          = NULL;

/* write functions for profiling */
static int ( *_MPI_File_write_all )  ( MPI_File, const void*, int, MPI_Datatype, MPI_Status* )            = NULL;
static int ( *_MPI_File_write_at_all)( MPI_File, MPI_Offset, const void*, int, MPI_Datatype, MPI_Status*) = NULL;

#if defined(_WITH_MPE_)
extern "C"
{
        /* for hints injection and transparent open/close modification */
        int __real_MPI_Init( int*, char*** );
        int __real_MPI_Init_thread( int*, char***, int, int* );
        int __real_MPI_Finalize( void );
        int __real_MPI_File_open( MPI_Comm, const char*, int, MPI_Info, MPI_File* );
        int __real_MPI_File_close( MPI_File* );
        int __real_MPI_File_write_all( MPI_File, const void*, int, MPI_Datatype, MPI_Status*);
        int __real_MPI_File_write_at_all( MPI_File, MPI_Offset, const void*, int, MPI_Datatype, MPI_Status*);
}
#elif defined(_WITH_DARSHAN_)
extern "C" void *(*dlsym( void* handle, const char* symbol ))( );
#endif

/* the object that holds the configs */
static Json::Value config_;

/* hint type "hint name", "hint value" (e.g. "stripe_count", "4") */
typedef std::pair<std::string, std::string> hint_;

/* list of hints */
typedef std::list<hint_> hints_;

/* mpiwrap init status */
int initialised = 0;

/* map of paths and associated list of hints */
std::map <std::string, hints_*> * paths_ = NULL;

/* name of the file(s) starting an I/O cycle */
std::list<std::string> * guardnames_ = NULL;

/* list of currently open files */
std::list<MPI_File*> * ofiles_ = NULL;

/* timers */
double o_time, c_time, w_time, r_time, stim;

/* global communicator */
MPI_Comm gcom = MPI_COMM_NULL;

/* list of hints names (keys) */
std::string key_[] = {/* Generic hints */
	"ind_rd_buffer_size",
	"ind_wr_buffer_size",
	"romio_ds_read",
	"romio_ds_write",
	"cb_buffer_size",
	"cb_nodes",
	"romio_cb_read",
	"romio_cb_write",
	"romio_no_indep_rw",
	"cb_config_list",
	"striping_factor",
	"striping_unit",
	"start_iodevice",
	/* Lustre hints */
	"romio_lustre_co_ratio",
	"romio_lustre_coll_threshold",
	"romio_lustre_cb_ds_threshould",
	"romio_lustre_ds_in_coll",
	/* E10 hints */
	"e10_cache",
	"e10_cache_path",
	"e10_cache_flush_flag",
	"e10_cache_discard_flag",
	"e10_collective_mode",
	"e10_cache_threads"
};

static void MPI_Wrap_init( )
{
	int fd;
	unsigned int i, j, num_paths, num_files, numkeys;
	hint_ hint;
	hints_ *hints;
	Json::Reader reader;
	Json::Value nil;
	std::string *json;
	size_t len;
	char * buf;

	/* check mpiwrap status */
	if( initialised )
		return;

	/* initialise timers */
	o_time = c_time = w_time = 0.0;

	numkeys = ( unsigned int )sizeof( key_ ) / sizeof( std::string );

	/* initialize paths_ map */
	paths_ = new std::map<std::string, hints_*>( );

	/* initialize guardnames_ and ofiles_ lists */
	guardnames_ = new std::list<std::string>( );
	ofiles_ = new std::list<MPI_File*>( );

	/* read the json configuration file for the rules */
	char *config_file = getenv( "MPI_HINTS_CONFIG" );

	/* if config is not defined skip to api configuration */
	if( config_file == NULL )
	{
		std::cerr << "## ERROR: No configuration file available" << std::endl;
		std::cerr << "## type: 'export MPI_HINTS_CONFIG=\"config\"" << std::endl;
		std::cerr << "## now continuing without hints" << std::endl;
		goto fn_configure_api;
	}

	if( (fd = open( config_file, O_RDONLY )) == -1)
	{
		std::cerr << "## Error opening config file: " << strerror( errno ) << std::endl;
		exit( EXIT_FAILURE );
	}

	lseek( fd, 0, SEEK_END );
	len = lseek( fd, 0, SEEK_CUR );
	lseek( fd, 0, SEEK_SET );
	buf = ( char* )malloc( len );
	read( fd, ( char* )buf, len );
	close( fd );

	json = new std::string( buf );
	reader.parse( *json, config_ );
	delete json;
	free( buf );

	num_paths = config_["Directory"].size( );
	num_files = config_["File"].size( );

	/* Directory contains a set of MPI-IO hints for files inside a specific directory */
	for( i = 0; i < num_paths; i++ )
	{
		Json::Value jvalue = config_["Directory"].get( i, nil );
		std::string path = jvalue["Path"].asString( );
		std::string type = jvalue["Type"].asString( );

		hints = new hints_( );
		paths_->insert( std::pair<std::string, hints_*>( path, hints ) );

		for( j = 0; j < numkeys && !type.compare( std::string( "MPI" ) ); j++ )
			if( !jvalue[key_[j].c_str( )].isNull( ) )
			{
				hint = make_pair( key_[j].c_str( ), jvalue[key_[j].c_str( )].asString( ) );
				hints->push_back( hint );
			}
	}

	/* File contains a set of MPI-IO hints for a specific pathname or pathname prefix  */
	for( i = 0; i < num_files; i++ )
	{
		Json::Value jvalue = config_["File"].get( i, nil );
		std::string path = jvalue["Path"].asString( );
		std::string type = jvalue["Type"].asString( );

		hints = new hints_( );
		paths_->insert( std::pair<std::string, hints_*>( path, hints ) );

		for( j = 0; j < numkeys && !type.compare( std::string( "MPI" ) ); j++ )
			if( !jvalue[key_[j].c_str( )].isNull( ) )
			{
				hint = make_pair( key_[j].c_str( ), jvalue[key_[j].c_str( )].asString( ) );
				hints->push_back( hint );
			}
	}

	/* retrieve guardnames that identify a computation cycles boundary */
	if( config_["Guardnames"].asString( ) != "" )
	{
		std::stringstream ss( config_["Guardnames"].asString( ) );
		std::string item;
		while( std::getline( ss, item, ',' ) )
		{
			guardnames_->push_back( item );
		}
	}

fn_configure_api:
#if defined(_WITH_MPE_)
	/* redirect calls to mpe library (needs mpiwrap.sh) */
	_MPI_Init              = &__real_MPI_Init;
	_MPI_Init_thread       = &__real_MPI_Init_thread;
	_MPI_Finalize          = &__real_MPI_Finalize;
	_MPI_File_open         = &__real_MPI_File_open;
	_MPI_File_close        = &__real_MPI_File_close;
	_MPI_File_write_all    = &__real_MPI_File_write_all;
	_MPI_File_write_at_all = &__real_MPI_File_write_at_all;
#elif defined(_WITH_DARSHAN_)
	/* redirect calls to darshan (JUBE support) */
	_MPI_Init              = ( int (*)( int*, char*** ) )                                                  dlsym( RTLD_NEXT, "MPI_Init" );
	_MPI_Init_thread       = ( int (*)( int*, char***, int, int* ) )                                       dlsym( RTLD_NEXT, "MPI_Init_thread" );
	_MPI_Finalize          = ( int (*)( void ) )                                                           dlsym( RTLD_NEXT, "MPI_Finalize" );
	_MPI_File_open         = ( int (*)( MPI_Comm, const char*, int, MPI_Info, MPI_File* ) )                dlsym( RTLD_NEXT, "MPI_File_open" );
	_MPI_File_close        = ( int (*)( MPI_File* ) )                                                      dlsym( RTLD_NEXT, "MPI_File_close" );
	_MPI_File_write_all    = ( int (*)( MPI_File, const void*, int, MPI_Datatype, MPI_Status*)             dlsym( RTLD_NEXT, "MPI_File_write_all" );
	_MPI_File_write_at_all = ( int (*)( MPI_File, MPI_Offset, const void*, int, MPI_Datatype, MPI_Status*) dlsym( RTLD_NEXT, "MPI_File_write_at_all" );
#else
	/* redirect calls to mpi-io library */
	_MPI_Init              = &PMPI_Init;
	_MPI_Init_thread       = &PMPI_Init_thread;
	_MPI_Finalize          = &PMPI_Finalize;
	_MPI_File_open         = &PMPI_File_open;
	_MPI_File_close        = &PMPI_File_close;
	_MPI_File_write_all    = &PMPI_File_write_all;
	_MPI_File_write_at_all = &PMPI_File_write_at_all;
#endif
	initialised = 1;
}

/* set hints for filename */
static void setFileHints( const char *filename, MPI_Info *info )
{
	/* extract dirname for the file */
	char * pathname = strdup( filename );
	char * basedir  = dirname( ( char* )pathname );
	std::string path( pathname );
	std::string base( basedir );

	std::map<std::string, hints_ *>::iterator dir = paths_->find( base );
	std::map<std::string, hints_ *>::iterator file = paths_->find( path );
	std::list<hint_>::iterator it;

	/* Add the user hints to the info object using: */
	if( dir != paths_->end( ) )
	{
		/* Absolute directory name matching (e.g. /work/deep47/flash_io) */
		it = dir->second->begin( );
		for( ; it != dir->second->end( ); it++ )
		{
			PMPI_Info_set( *info, it->first.c_str( ), it->second.c_str( ) );
		}
	}
	else if( file != paths_->end( ) )
	{
		/* Absolute filename matching (e.g. /work/deep47/flash_io/flash_io_test_hdf5_chk_0000) */
		it = file->second->begin( );
		for( ; it != file->second->end( ); it++ )
		{
			PMPI_Info_set( *info, it->first.c_str( ), it->second.c_str( ) );
		}
	}
	else
	{
		/* Absolute filename prefix matching (e.g. /work/deep47/flash_io/flash_io_test_hdf5_chk_) */
		file = paths_->begin( );
		for( ; file != paths_->end( ); file++ )
		{
			if( !strncmp( file->first.c_str( ), filename, strlen( file->first.c_str( ) ) ) )
			{
				it = file->second->begin( );
				for( ; it != file->second->end( ); it++ )
				{
					PMPI_Info_set( *info, it->first.c_str( ), it->second.c_str( ) );
				}
				break;
			}
		}
	}

	free( pathname );
}

/* libmpiwrap initialization */
int MPI_Init( int *argc, char ***argv )
{
	/* initialise mpiwrap */
	MPI_Wrap_init( );

	/* call mpi init */
	int ret = _MPI_Init( argc, argv );

	/* start timer */
	r_time = 0.0 - PMPI_Wtime( );

	return ret;
}

int MPI_Init_thread( int *argc, char ***argv, int required, int *provided )
{
	/* initialise mpiwrap */
	MPI_Wrap_init( );

	/* call mpi init */
	int ret = _MPI_Init_thread( argc, argv, required, provided );

	/* start timer */
	r_time = 0.0 - PMPI_Wtime( );

	return ret;
}

int MPI_Finalize( void )
{
	/* delete paths_ that have received hints */
	std::map<std::string, hints_*>::iterator it = paths_->begin( );
	for( ; it != paths_->end( ); it++ )
	{
		delete it->second;
	}
	delete paths_;

	/* delete guardnames_ and ofiles_ if any */
	while( !guardnames_->empty( ) )
		guardnames_->pop_front( );
	delete guardnames_;

	/* first close the files still opened */
	while( !ofiles_->empty( ) )
	{
		/* get MPI file handle */
		MPI_File *local_fh = ofiles_->front( );

		stim = MPI_Wtime( );

		/* close the corresponding file */
		_MPI_File_close( local_fh );

		c_time += MPI_Wtime( ) - stim;

		/* free the handle */
		if( local_fh )
			free( local_fh );

		/* remove the handle from the list */
		ofiles_->pop_front( );
	}
	delete ofiles_;

	/* log profiling info */
	r_time += PMPI_Wtime( );
	double max_w_time, max_o_time, max_c_time, max_r_time;
	int myrank;

	PMPI_Allreduce( &w_time, &max_w_time, 1, MPI_DOUBLE, MPI_MAX, MPI_COMM_WORLD );
	PMPI_Allreduce( &o_time, &max_o_time, 1, MPI_DOUBLE, MPI_MAX, MPI_COMM_WORLD );
	PMPI_Allreduce( &c_time, &max_c_time, 1, MPI_DOUBLE, MPI_MAX, MPI_COMM_WORLD );
	PMPI_Allreduce( &r_time, &max_r_time, 1, MPI_DOUBLE, MPI_MAX, MPI_COMM_WORLD );

	PMPI_Comm_rank( MPI_COMM_WORLD, &myrank );

	if( myrank == 0 )
	{
		fprintf(stdout, "open,write,close,runtime\n");
		fprintf(stdout, "%f,%f,%f,%f\n", max_o_time, max_w_time, max_c_time, max_r_time);
	}

	//fprintf(stdout, "rank=%d: open=%f\n", myrank, o_time);

	/* call mpi finalize */
	int ret = _MPI_Finalize( );

	/* reset function pointers */
	_MPI_Init_thread = NULL;
	_MPI_Init = NULL;
	_MPI_Finalize = NULL;
	_MPI_File_open = NULL;
	_MPI_File_close = NULL;
	_MPI_File_write_all = NULL;
	_MPI_File_write_at_all = NULL;

	/* reset mpiwrap status */
	initialised = 0;

	return ret;
}

int MPI_File_open( MPI_Comm comm, const char *filename, int amode, MPI_Info info, MPI_File *fh )
{
	int ret, myrank;
	MPI_Info myinfo;
	MPI_File *local_fh;
	char *absfilename = realpath(filename, NULL);

	PMPI_Comm_rank( comm, &myrank );

	/* make sure fh is not null */
	assert( fh );

	/* if info is null create myinfo */
	if( info == MPI_INFO_NULL )
	{
        	PMPI_Info_create( &myinfo );
	}
	else /* otherwise overwrite info */
	{
		myinfo = info;
	}

	/* get the filename's base name */
	char * pathname = strdup( absfilename );
	char * basenam  = basename( pathname );

	std::list<std::string>::iterator git = guardnames_->begin( );
	for( ; git != guardnames_->end( ); git++ )
	{
		/* if the filename's base name is a guardname */
		if( !strncmp( git->c_str( ), basenam, strlen( git->c_str( ) ) ) )
		{
			/* close the files opened so far (if any) */
			while( !ofiles_->empty( ) )
			{
				/* get MPI file handle */
				local_fh = ofiles_->front( );

				/* start timer */
				stim = PMPI_Wtime( );

				/* close the corresponding file */
				if( (ret = _MPI_File_close( local_fh )) != MPI_SUCCESS )
					fprintf( stderr, "[%s] Error while closing a file under synchronisation!\n",
						__FILE__ );

				/* stop timer */
				c_time += (PMPI_Wtime( ) - stim);

				/* free the handle */
				if( local_fh )
					free( local_fh );

				/* remove the handle from the list */
				ofiles_->pop_front( );
			}
			break;
		}
	}

	/* set hints for filename */
	setFileHints( absfilename, &myinfo );

	/* rank 0 prints hints */
	if( myrank == 0 )
	{
		int flag, i, numkeys = ( unsigned int )sizeof( key_ ) / sizeof( std::string );
		char *value = ( char* )malloc( MPI_MAX_INFO_VAL + 1);
		std::cout << "Filename " << absfilename << " {" << std::endl;
		for( i = 0; i < numkeys; i++ )
		{
			PMPI_Info_get( myinfo, key_[i].c_str( ), MPI_MAX_INFO_VAL, value, &flag );
			if( flag )
				std::cout << "\t" << key_[i] << ": " << value << std::endl;
		}
		std::cout << "}" << std::endl;
		free( value );
	}

	/* start timer */
	stim = PMPI_Wtime( );

	/* open filename */
	if( (ret = _MPI_File_open( comm, absfilename, amode, myinfo, fh )) != MPI_SUCCESS )
		goto fn_exit;

	/* stop timer */
	o_time += (PMPI_Wtime( ) - stim);

	/* if filename is a guardname insert its file handle in the ofiles_ list */
	git = guardnames_->begin( );
	for( ; git != guardnames_->end( ); git++ )
	{
		/* if the current file is a guardname file */
		if( !strncmp( git->c_str( ), basenam, strlen( git->c_str( ) ) ) )
		{
			/* Since the original fh can be statically allocated (i.e.
			 * have a stack address), we need to extract the MPI_File
			 * handle returned by MPI_File_open and store it in the
			 * heap for future references */
			local_fh = ( MPI_File* )malloc( sizeof( MPI_File ) );
			*local_fh = *fh;
			ofiles_->push_back( local_fh );
			break;
		}
	}

	/* if filename is not a guardname but follows one (i.e. is a file that
	 * belongs to a computation cycle), insert its file handle in the ofiles_
	 * list */
	if( git == guardnames_->end( ) && !ofiles_->empty( ) )
	{
		local_fh = ( MPI_File* )malloc( sizeof( MPI_File ) );
		*local_fh = *fh;
		ofiles_->push_back( local_fh );
	}

fn_exit:
	if( info == MPI_INFO_NULL )
	{
		PMPI_Info_free( &myinfo );
	}

	free( pathname );

	return ret;
}

int MPI_File_close( MPI_File *fh )
{
	int ret = MPI_SUCCESS;

	/* start timer */
	stim = PMPI_Wtime( );

	if( ofiles_->empty( ) )
	{
		ret = _MPI_File_close( fh );
	}

	/* stop timer */
	c_time = (PMPI_Wtime( ) - stim);

	return ret;
}

int MPI_File_write_all( MPI_File fh, const void *buf, int count, MPI_Datatype datatype, MPI_Status *status )
{
	int ret;

	/* start timer */
	stim = PMPI_Wtime( );

	ret  = _MPI_File_write_all( fh, buf, count, datatype, status );

	/* stop timer */
	w_time += (PMPI_Wtime( ) - stim);

	return ret;
}

int MPI_File_write_at_all( MPI_File fh, MPI_Offset off, const void *buf, int count, MPI_Datatype datatype, MPI_Status *status )
{
	int ret;

	/* start timer */
	stim = PMPI_Wtime( );

	ret = _MPI_File_write_at_all( fh, off, buf, count, datatype, status );

	/* stop timer */
	w_time += (PMPI_Wtime( ) - stim);

	return ret;
}

/*
 * vim: st=8 sts=8 sw=8 noexpandtab
 */
