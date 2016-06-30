# Makefile for the I/O benchmark -- generic linux 
.PHONY: flash_benchmark_io

# objects
OBJS = get_mfluid_property.o \
       flash_release.o \
       flash_benchmark_io.o 

OBJS_HDF5_PARALLEL_IO = checkpoint_hdf5_parallel.o \
                        plotfile_hdf5_parallel.o \
                        h5_file_interface.o \
                        h5_parallel_write.o \
                        h5_parallel_write_single.o

# compiler and linker commands
FCOMP   = mpif90
CCOMP   = mpicc
CPPCOMP = mpiCC
LINK    = mpif90 


# library locations
HDF5path = $(INSTALL_DIR)
ZLIBpath = /usr

# compiler flags
FFLAGS = -c -g -O2 \
         -DN_DIM=3 -DMAXBLOCKS=100 -DIONMAX=13 #-DTIMERS 

F90FLAGS = 

CFLAGS = -c -g -O2 -DTFLOPS -DN_DIM=3 -DH5Dcreate_vers=1 


# linker flags
LFLAGS = -o 


# libraries to include
LIB = \
      -L$(HDF5path)/lib -lhdf5 \
      -L$(ZLIBpath)/lib64 -lz -L/usr/lib -lc -lm -ldl \
      -L$(INSTALL_DIR)/lib -lmpiwrap -ljson \
      -L$(INSTALL_DIR)/lib -llmpe -lmpe -lmpe_f2cmpi
 
LDFLAGS = #\
      -Wl,--wrap=MPI_Init,--wrap=MPI_Finalize,--wrap=MPI_File_open,--wrap=MPI_File_close

.SUFFIXES: .f .F .f90 .F90 .c .C .o

.F90.o :
	$(FCOMP) $(FFLAGS) $<

.f.o :
	$(FCOMP) $(FFLAGS) $<

.c.o :
	$(CCOMP) $(CFLAGS) $<

flash_benchmark_io: $(OBJS) \
                    $(OBJS_HDF5_PARALLEL_IO) 
	$(LINK) $(LFLAGS) $@ $(OBJS) $(OBJS_HDF5_PARALLEL_IO) $(LDFLAGS) $(LIB)

clean:
	rm *.o flash_benchmark_io
