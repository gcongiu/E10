      program flash_benchmark_io
!
! This is a sample program that setups the FLASH data structures and 
! drives the I/O routines.  It is intended for benchmarking the I/O
! performance.
! 

! the main data structures are contained in common blocks, defined in the
! include files

#include "common.fh"

      integer ierr
      integer i, iterations
      
      real*4 time_io, time_begin, delay

      integer, parameter :: local_blocks = 0.8*maxblocks
      character(len=255) :: cwd
      character(len=32) :: arg

      iterations = 1
! get command line arguments
      do i = 1, iargc()
         call getarg(i, arg)
         if (arg .eq. "-i") then
            call getarg(i+1, arg)
            read (arg,*) iterations
         else if (arg .eq. "-d") then
            call getarg(i+1, arg)
            read (arg,*) delay
         endif 
      enddo

! initialize MPI and get the rank and size
      call MPI_INIT(ierr)

      call MPI_Comm_Rank (MPI_Comm_World, MyPE, ierr)
      call MPI_Comm_Size (MPI_Comm_World, NumPEs, ierr)

      MasterPE = 0

      if (MyPE .EQ. MasterPE) then
! print some initialization information
         print *, 'HDF 5 v 1.4 test on Blue Pacific'
         print *, NumPEs, ' processors'
         print *, 'nxb, nyb, nzb = ', nxb, nyb, nzb
         print *, 'nguard = ', nguard
         print *, 'number of blocks = ', local_blocks
         print *, 'nvar = ', nvar
         print *, 'iterations = ', iterations
         print *, 'delay = ', delay
      endif


! put ~100 blocks on each processor -- make it vary a little, since it does
! in the real application.  This is the maximum that we can fit on Blue 
! Pacific comfortably.
      lnblocks = local_blocks + mod(MyPE,3)

! just fill the tree stucture with dummy information -- we are just going to
! dump it out
      size(:,:) = 0.5e0
      lrefine(:) = 1
      nodetype(:) = 1
      refine(:) = .FALSE.
      derefine(:) = .FALSE.
      parent(:,:) = -1
      child(:,:,:) = -1
      coord(:,:) = 0.25e0
      bnd_box(:,:,:) = 0.e0
      neigh(:,:,:) = -1
      empty(:) = 0

! initialize the unknowns with the index of the variable
      do i = 1, nvar
        unk(i,:,:,:,:) = float(i)
      enddo

! setup the file properties
      call getcwd(cwd)
      basenm = trim(cwd) // "/flash_io_test_"
      
#ifdef CHIBA
#error "change 'basenm' to the correct basename for your file system"
      ! examples: 
      ! basenm = "pvfs:/stopvfsvol/flash_io_test_"
      ! basenm = "pvfs2:/stopvfsvol/flash_io_test_"
#endif      

#ifdef BGL
      basenm = "/pvfs-surveyor/robl/flash"
#endif

#ifdef VIRGO
#error "change 'basenm' to the correct basename for your file system"
      !basenm = "/mnt/pvfs/flash_io_bench/hdf5/flash_io_test_"
#endif

      do i = 0, iterations-1
!---------------------------------------------------------------------------
! HDF 5 checkpoint file
!---------------------------------------------------------------------------
         if (MyPE .EQ. MasterPE) then
            print *, ' '
            print *, 'doing HDF 5 parallel I/O to a single file'
         endif

         !time_begin = MPI_Wtime()
         call cpu_time(time_begin)
         call checkpoint_wr_hdf5_par(i,0.e0)
         !time_io = MPI_Wtime() - time_begin
         call cpu_time(time_io)

         if (MyPE .EQ. MasterPE) then
            print *, 'time to output = ', time_io - time_begin
         endif

!---------------------------------------------------------------------------
! HDF 5 plotfile -- no corners
!---------------------------------------------------------------------------
         if (MyPE .EQ. MasterPE) then
            print *, ' '
            print *, 'writing an HDF 5 plotfile --  no corners'
         endif

         !time_begin = MPI_Wtime()
         call cpu_time(time_begin)
         !call plotfile_hdf5_par(i,0.e0,.false.)
         !time_io = MPI_Wtime() - time_begin
         call cpu_time(time_io)
    
         if (MyPE .EQ. MasterPE) then
            print *, 'time to output = ', time_io - time_begin
         endif
!---------------------------------------------------------------------------
! HDF 5 plotfile -- corners
!---------------------------------------------------------------------------
         if (MyPE .EQ. MasterPE) then
            print *, ' '
            print *, 'writing an HDF 5 plotfile --  corners'
         endif

         !time_begin = MPI_Wtime()
         call cpu_time(time_begin)
         !call plotfile_hdf5_par(i,0.e0,.true.)
         !time_io = MPI_Wtime() - time_begin
         call cpu_time(time_io)
    
         if (MyPE .EQ. MasterPE) then
            print *, 'time to output = ', time_io - time_begin
         endif
! emulate computation delay
         !time_begin = MPI_Wtime()
         call cpu_time(time_begin)
         do 
            !time_io = MPI_Wtime() - time_begin
            call cpu_time(time_io)
            if (time_io - time_begin >= delay) exit
         enddo
      enddo
      
      call MPI_Finalize(ierr)
      end
