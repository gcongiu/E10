/* This file contains the routines that open and close the HDF5 files */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pnetcdf.h>
#include "ncmpi_flash.h"
#include <mpi.h>

#ifdef BGL
#include "bgl_compat.h"
#endif

/* define an info object to store MPI-IO information */
static MPI_Info FILE_INFO_TEMPLATE;


/* xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx */

void ncmpi_initialize_file_(int* file_identifier, char filename[81])
{
  int ierr, status;

  /* make the filename pretty -- cutoff any trailing white space */
  int len = 0;
  char* string_index; 

  /* operate on a copy of the filename -- adding the null character for
     C messes with FORTRAN */

  char local_filename[81];


  string_index = filename;
  
  while (*string_index != ' ') {
    local_filename[len] = filename[len];
    len++;
    string_index++;
  }

  *(local_filename+len) = '\0';

  /* ---------------------------------------------------------------------
      platform dependent code goes here -- the access template must be
      tuned for a particular filesystem blocksize.  some of these 
      numbers are guesses / experiments, others come from the file system
      documentation.

      The sieve_buf_size should be equal a multiple of the disk block size
     ---------------------------------------------------------------------- */

  /* create an MPI_INFO object -- on some platforms it is useful to
     pass some information onto the underlying MPI_File_open call */
  ierr = MPI_Info_create(&FILE_INFO_TEMPLATE);

#ifdef TFLOPS
  ierr = MPI_Info_set(FILE_INFO_TEMPLATE, "access_style", "write_once");
  ierr = MPI_Info_set(FILE_INFO_TEMPLATE, "collective_buffering", "true");
  ierr = MPI_Info_set(FILE_INFO_TEMPLATE, "cb_block_size", "1048576");
  ierr = MPI_Info_set(FILE_INFO_TEMPLATE, "cb_buffer_size", "4194304");
#endif

#ifdef CHIBA
  ierr = MPI_Info_set(FILE_INFO_TEMPLATE, "access_style", "write_once");
  ierr = MPI_Info_set(FILE_INFO_TEMPLATE, "collective_buffering", "true");
  ierr = MPI_Info_set(FILE_INFO_TEMPLATE, "cb_block_size", "1048576");
  ierr = MPI_Info_set(FILE_INFO_TEMPLATE, "cb_buffer_size", "4194304");
#endif

#ifdef DEBUG_IO
  printf("set fapl to use MPI-IO, ierr = %d\n", (int) ierr);
#endif

  /* ----------------------------------------------------------------------
      end of platform dependent properties
     ---------------------------------------------------------------------- */

  /* create the file collectively */
#ifdef PNETCDF_VERSION_MAJOR 
#if ((PNETCDF_VERSION_MAJOR > 1) ||  (PNETCDF_VERSION_MAJOR==1 && PNETCDF_VERSION_MINOR >= 1))
  status = ncmpi_create(MPI_COMM_WORLD, local_filename, NC_CLOBBER|NC_64BIT_OFFSET|NC_64BIT_DATA, FILE_INFO_TEMPLATE, file_identifier); 
#else
#endif
#else
  status = ncmpi_create(MPI_COMM_WORLD, local_filename, NC_CLOBBER|NC_64BIT_OFFSET, FILE_INFO_TEMPLATE, file_identifier); 
#endif

#ifdef DEBUG_IO
  printf("openned the file, identifier = %d\n", (int) *file_identifier);
#endif
}



/* xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx */

void ncmpi_close_file_(int* file_identifier)
{
  int ierr, status;

  /* close the file */
  status = ncmpi_close(*file_identifier);
  
  ierr = MPI_Info_free(&FILE_INFO_TEMPLATE);
  
}
