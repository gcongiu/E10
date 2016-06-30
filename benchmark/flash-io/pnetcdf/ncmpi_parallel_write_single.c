/* This file contains the functions that write the data to the HDF5 file
 * The functions accept the PARAMESH data through arguments, since C cannot
 * handle common blocks 
 */

/* if the following flag is defined, status checking on the writes will
   be performed, and the results will be output to stdout */

/* #define DEBUG_IO */


#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <pnetcdf.h>
#include "ncmpi_flash.h"
#include <mpi.h>

#ifdef BGL
#include "bgl_compat.h"
#endif


/* xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx */

void ncmpi_write_header_info_sp_(int* MyPE,
			      int* nvar_out,            /* num vars to store */
			      int* file_identifier,   /* file handle */
			      char file_creation_time[],/* time / date stamp */
                              char flash_version[],     /* FLASH version num */
                              int* total_blocks,        /* total # of blocks */
                              double* time,              /* simulation time */
                              float* timestep,          /* current timestep */
                              int* nsteps,              /* # of timestep */
                              int nzones_block[3],      /* nxb, nyb, nzb */
                              char unk_labels[][4],     /* unknown labels */
			      int varid[])		/* output: var ids */
{
	int i,k;
        int ncid, status;
        int rank;
        int dim_tot_blocks, dim_nxb, dim_nyb, dim_nzb;
        int dim_NGID, dim_NDIM, dim_2;
        int dimids[4];
        char record_label[5];
        int string_size = 40;

        ncid = *file_identifier;
        status = ncmpi_def_dim(ncid, "dim_tot_blocks", (MPI_Offset)*total_blocks, &dim_tot_blocks);
	if(status != NC_NOERR) handle_error(status);
        status = ncmpi_def_dim(ncid, "dim_nxb", (MPI_Offset)nzones_block[0], &dim_nxb);
        status = ncmpi_def_dim(ncid, "dim_nyb", (MPI_Offset)nzones_block[1], &dim_nyb);
        status = ncmpi_def_dim(ncid, "dim_nzb", (MPI_Offset)nzones_block[2], &dim_nzb);
        status = ncmpi_def_dim(ncid, "dim_NGID", (MPI_Offset)NGID, &dim_NGID);
        status = ncmpi_def_dim(ncid, "dim_NDIM", (MPI_Offset)NDIM, &dim_NDIM);
        status = ncmpi_def_dim(ncid, "dim_2", (MPI_Offset)2, &dim_2);

	if(status != NC_NOERR) handle_error(status);
        dimids[0] = dim_tot_blocks;

        /* define var for refinement level */
        rank = 1;
        status = ncmpi_def_var (ncid, "lrefine", NC_INT, rank, dimids, varid+0);

        /* define var for nodetype */
        rank = 1;
        status = ncmpi_def_var (ncid, "nodetype", NC_INT, rank, dimids, varid+1);

        /* define var for global id */
        rank = 2;
        dimids[1] = dim_NGID;
        status = ncmpi_def_var (ncid, "gid", NC_INT, rank, dimids, varid+2);

        /* define var for grid coordinates */
        rank = 2;
        dimids[1] = dim_NDIM;
        status = ncmpi_def_var (ncid, "coordinates", NC_FLOAT, rank, dimids, varid+3);

        /* define var for grid block size */
        rank = 2;
        dimids[1] = dim_NDIM;
        status = ncmpi_def_var (ncid, "blocksize", NC_FLOAT, rank, dimids, varid+4);

        /* define var for grid bounding box */
        rank = 3;
        dimids[1] = dim_NDIM;
        dimids[2] = dim_2;
        status = ncmpi_def_var (ncid, "bndbox", NC_FLOAT, rank, dimids, varid+5);

        /* define var for unknown array */
        rank = 4;
        dimids[1] = dim_nzb;
        dimids[2] = dim_nyb;
        dimids[3] = dim_nxb;
        for (i=0; i<*nvar_out; i++) {
	strncpy(record_label, unk_labels[i], 4);
	for(k=0; k<=4; k++) {
                if(record_label[k] == ' ') {
                        record_label[k] = '_';
                        }
                }
          *(record_label+4) = '\0';
          status = ncmpi_def_var (ncid, record_label, NC_FLOAT, rank, dimids, varid+6+i);
	if(status != NC_NOERR) handle_error(status);
        }

	status = ncmpi_put_att_text(ncid, NC_GLOBAL, "file_creation_time", string_size, file_creation_time);
        status = ncmpi_put_att_text(ncid, NC_GLOBAL, "flash_version",  string_size, flash_version);
        status = ncmpi_put_att_int(ncid, NC_GLOBAL, "total_blocks",  NC_INT, 1, total_blocks);
        status = ncmpi_put_att_int(ncid, NC_GLOBAL, "nsteps", NC_INT, 1, nsteps);
        status = ncmpi_put_att_int(ncid, NC_GLOBAL, "nxb", NC_INT, 1, &nzones_block[0]);
        status = ncmpi_put_att_int(ncid, NC_GLOBAL, "nyb", NC_INT, 1, &nzones_block[1]);
        status = ncmpi_put_att_int(ncid, NC_GLOBAL, "nzb", NC_INT, 1, &nzones_block[2]);
        status = ncmpi_put_att_double(ncid, NC_GLOBAL, "time", NC_DOUBLE, 1, time);

        status = ncmpi_enddef(ncid);
	if(status != NC_NOERR) handle_error(status);
}


/* xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx */
void ncmpi_write_lrefine_sp_(int* file_identifier,
                       int* varid,
                       int lrefine[],
                       int* local_blocks,
                       int* total_blocks,
                       int* global_offset)
{


  int ncid, status;

  MPI_Offset start_1d, count_1d;

  ncid = *file_identifier;

  start_1d = (MPI_Offset) (*global_offset);
  count_1d = (MPI_Offset) (*local_blocks);


  status = ncmpi_put_vara_int_all(ncid, *varid, &start_1d, &count_1d, lrefine);
}

void ncmpi_write_nodetype_sp_(int* file_identifier,
                        int* varid,
                        int nodetype[],
                        int* local_blocks,
                        int* total_blocks,
                        int* global_offset)
{
  int ncid, status;

  MPI_Offset start_1d, count_1d;

  ncid = *file_identifier;

  start_1d = (MPI_Offset) (*global_offset);
  count_1d = (MPI_Offset) (*local_blocks);

  status = ncmpi_put_vara_int_all(ncid, *varid, &start_1d, &count_1d, nodetype);
}


/* xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx */

void ncmpi_write_gid_sp_(int* file_identifier,
                   int* varid,
                   int global_id[][NGID],
                   int* local_blocks,
                   int* total_blocks,
                   int* global_offset)
{
  int ncid, status;

  MPI_Offset start_2d[2], count_2d[2];

  ncid = *file_identifier;

  start_2d[0] = (MPI_Offset) (*global_offset);
  start_2d[1] = 0;

  count_2d[0] = (MPI_Offset) (*local_blocks);
  count_2d[1] = NGID;

  status = ncmpi_put_vara_int_all(ncid, *varid, start_2d, count_2d, *global_id);
}

void ncmpi_write_coord_sp_(int* file_identifier,
			int* varid,
		        float* coordinates,
		        int* local_blocks,
		        int* total_blocks,
		        int* global_offset)
{
  int ncid, status;

  MPI_Offset start_2d[2], count_2d[2];

  ncid = *file_identifier;

  /* create the hyperslab -- this will differ on the different processors */
  start_2d[0] = (MPI_Offset) (*global_offset);
  start_2d[1] = 0;

  count_2d[0] = (MPI_Offset) (*local_blocks);
  count_2d[1] = NDIM;

  ncmpi_put_vara_float_all(ncid, *varid, start_2d, count_2d, coordinates);
}


/* xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx */

void ncmpi_write_size_sp_(int* file_identifier,
		       int* varid,
		       float* size,
		       int* local_blocks,
		       int* total_blocks,
		       int* global_offset)
{
  int ncid, status;
  MPI_Offset start_2d[2], count_2d[2];

  ncid = *file_identifier;

  /* create the hyperslab -- this will differ on the different processors */
  start_2d[0] = (MPI_Offset) (*global_offset);
  start_2d[1] = 0;

  count_2d[0] = (MPI_Offset) (*local_blocks);
  count_2d[1] = NDIM;


  ncmpi_put_vara_float_all(ncid, *varid, start_2d, count_2d, size);
}


/* xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx */

void ncmpi_write_bnd_box_sp_(int* file_identifier,
			  int* varid,
			  float* bnd_box,
			  int* local_blocks,
			  int* total_blocks,
			  int* global_offset)
{
  int ncid, status;

  MPI_Offset start_3d[3], count_3d[3];

  ncid = *file_identifier;

  /* create the hyperslab -- this will differ on the different processors */
  start_3d[0] = (MPI_Offset) (*global_offset);
  start_3d[1] = 0;
  start_3d[2] = 0;

  count_3d[0] = (MPI_Offset) (*local_blocks);
  count_3d[1] = NDIM;
  count_3d[2] = 2;

  ncmpi_put_vara_float_all(ncid, *varid, start_3d, count_3d, bnd_box);
}



/* 
   This function writes out a single unknown (passed from the checkpoint 
   routine), giving the record a label from the varnames or species
   database 
   
   The dimensions of the unknowns array (nvar, nxb, nyb, nzb, maxblocks)
   are passed through as arguments.  The dataspace (what gets written
   to disk) and the memory space (how the unknowns array is accessed in
   local memory) are defined based on these passed values.  This allows
   use to pass an unk array that has all the guardcells + interior cells
   (as in the checkpointing), or a dummy unk array that has just the 
   interior cells (in this case, nguard would be passed as 0).
*/

void ncmpi_write_unknowns_sp_(int* file_identifier,
			   int* varid,
			   int* nxb,            /* # of zones to store in x */
			   int* nyb,            /* # of zones to store in y */
			   int* nzb,            /* # of zones to store in z */
			   float* unknowns,   /* [mblk][NZB][NYB][NXB][nvar] */
			   char record_label[5],/* add char-null termination */
			   int* local_blocks,  
			   int* total_blocks,
			   int* global_offset)
{
  int ncid, status;

  MPI_Offset start_4d[4], count_4d[4];

  ncid = *file_identifier;

  /* create the hyperslab -- this will differ on the different processors */
  start_4d[0] = (MPI_Offset) (*global_offset);
  start_4d[1] = 0;
  start_4d[2] = 0;
  start_4d[3] = 0;

  count_4d[0] = (MPI_Offset) (*local_blocks);
  count_4d[1] = *nzb;
  count_4d[2] = *nyb;
  count_4d[3] = *nxb;

  ncmpi_put_vara_float_all(ncid, *varid, start_4d, count_4d, unknowns);
}





