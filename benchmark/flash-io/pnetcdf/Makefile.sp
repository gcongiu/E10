# Makefile for the I/O benchmark -- IBM SP2

# objects
OBJS = get_mfluid_property.o \
       flash_release.o \
       flash_benchmark_io.o 

OBJS_NCMPI_PARALLEL_IO = checkpoint_ncmpi_parallel.o \
                         plotfile_ncmpi_parallel.o \
                         ncmpi_file_interface.o \
                         ncmpi_parallel_write.o \
                         ncmpi_parallel_write_single.o

# compiler and linker commands
FCOMP   = mpxlf
CCOMP   = mpcc_r
CPPCOMP = mpCC_r
LINK    = mpcc_r


# library locations
NCMPIpath = /rmount/paci06/nwestern/ux453343/soft/mpinetcdf

# compiler flags
FFLAGS = -c -O3 -qintsize=4 -qrealsize=8 -cpp -qtune=auto \
         -WF,-DN_DIM=3 -WF,-DMAXBLOCKS=100, -WF,-DIONMAX=13 

F90FLAGS = -qsuffix=f=F90:cpp=F90 -qfree

CFLAGS = -c -O3 -I $(NCMPIpath)/include -DIBM -DN_DIM=3 -qtune=auto

.SUFFIXES: .f .F .f90 .F90 .c .C .o

# linker flags
LFLAGS = -brename:.ncmpi_close_file_,.ncmpi_close_file \
         -brename:.ncmpi_initialize_file_,.ncmpi_initialize_file \
         -brename:.ncmpi_write_header_info_,.ncmpi_write_header_info \
         -brename:.ncmpi_write_lrefine_,.ncmpi_write_lrefine \
         -brename:.ncmpi_write_nodetype_,.ncmpi_write_nodetype \
         -brename:.ncmpi_write_gid_,.ncmpi_write_gid \
         -brename:.ncmpi_write_coord_,.ncmpi_write_coord \
         -brename:.ncmpi_write_size_,.ncmpi_write_size \
         -brename:.ncmpi_write_bnd_box_,.ncmpi_write_bnd_box \
         -brename:.ncmpi_write_bnd_box_min_,.ncmpi_write_bnd_box_min \
         -brename:.ncmpi_write_bnd_box_max_,.ncmpi_write_bnd_box_max \
         -brename:.ncmpi_write_unknowns_,.ncmpi_write_unknowns \
         -brename:.ncmpi_write_header_info_sp_,.ncmpi_write_header_info_sp \
         -brename:.ncmpi_write_coord_sp_,.ncmpi_write_coord_sp \
         -brename:.ncmpi_write_size_sp_,.ncmpi_write_size_sp \
         -brename:.ncmpi_write_bnd_box_sp_,.ncmpi_write_bnd_box_sp \
         -brename:.ncmpi_write_bnd_box_min_sp_,.ncmpi_write_bnd_box_min_sp \
         -brename:.ncmpi_write_bnd_box_max_sp_,.ncmpi_write_bnd_box_max_sp \
         -brename:.ncmpi_write_unknowns_sp_,.ncmpi_write_unknowns_sp \
         -o


# libraries to include
LIB = -L $(NCMPIpath)/lib -lnetcdf \
      -L /usr/local/lib -lz -bmaxdata:0x80000000 -bmaxstack:0x10000000 \
      -lxlf -lxlfutil -lxlf90

.SUFFIXES: .f .F .f90 .F90 .c .C .o

.F90.o :
	$(FCOMP) $(FFLAGS) $(F90FLAGS) $<

.c.o :
	$(CCOMP) $(CFLAGS) $<

flash_benchmark_io: $(OBJS) \
		    $(OBJS_NCMPI_PARALLEL_IO)
	$(LINK) $(LFLAGS) $@ $(OBJS) $(OBJS_NCMPI_PARALLEL_IO) $(LIB)









