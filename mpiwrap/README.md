MPIWRAP v0.1

#Description
MPIWRAP is a wrapper library for MPI. Using MPIWRAP, MPI-IO hints can be passed to the
underlying ROMIO layer transparently, without any need of modifying the application''s
source code. Users can define the set of MPI-IO hints they wish to use in a configuration
file (structured using the json format) and pass this file to MPIWRAP using an env
variable.


#Requirements
MPIWRAP requires libjsoncpp-dev.


#Compiling and installing MPIWRAP
MPIWRAP can work in three different modes. Each mode can be selected using a compilation
flag. In particular MPIWRAP supports:

  * Stand alone mode: in this case the application is not using any additional library
    for performance profiling or operation logging. MPIWRAP will override `MPI_File_open()`
    and `MPI_File_close()` operations redirecting calls to `PMPI_File_open()` and `PMPI_File_close()`.
    Compile and install as follows:

                $ make && make install

  * MPE compatibility mode: in this case the application is using the MPE profiling library.
    MPE is a library containing logging features and typically is linked statically to the
    MPI application. MPE already overrides the `MPI_*` symbols (including `MPI_File_open()` and
    `MPI_File_close()`). In order to provide compatibility and make sure MPIWRAP is the library
    intercepting MPI calls we need to redefine `MPI_File_open()` and `MPI_File_close()` in the
    liblmpe.a library. This can be done using the mpiwrap.sh script provided with the
    package. Compile and install as follows:

                $ make with_mpe && make install
                $ bash mpiwrap.sh -w -f $INSTALL_DIR/lib/liblmpe.a

  * Darshan compatibility mode: in this case the application is using the Darshan profiling
    library. Darshan is typically compiled and linked dynamically to the MPI application.
    Some time the library is preloaded using LD_PRELOAD in order to make its employment
    transparent to the application. MPI calls intercepted by MPIWRAP can be redirected
    to Darshan calls using the ldsym mechanism. Compile and install as follows:

                $ make with_darshan && make install


#Compiling applications
Applications use MPIWRAP by simply adding the `libmpiwrap.so` library in the list of dependent
libraries, i.e.:

                $ gcc -o main main.cc -L$INSTALL_DIR/lib -lmpiwrap


#Running applications
Users can pass the configuration file containing to the application by exporting its full
pathname to the `MPI_HINTS_CONFIG` environment variable:

                $ export MPI_HINTS_CONFIG=$PATH_TO_CONF

MPIWRAP will load the configuration file information when the MPI library is initialised
(i.e. at `MPI_Init()` call). Afterwards, every opened file will be checked against the
configuration file to see if there are hints defined for it. If this is the case, hints
will be passed to the `PMPI_File_open()` routine.


#Additional features
To support E10 hints MPIWRAP can also modify the open/close sequence for files. In particular
for checkpoint files, identified as `Guardnames` in the configuration file, the close operation
will be postponed before the open of the next checkpoint file. This is done to allow the E10
implementation to synchronise cached data in background during computation. More information
can be found in the source code (`mpiwrap.cpp`).


An example of configuration file is provided with the package and can be found in this folder.

For any question write to the author at giuseppe.congiu@seagate.com
