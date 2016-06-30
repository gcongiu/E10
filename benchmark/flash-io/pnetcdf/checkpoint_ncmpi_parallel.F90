!----------------------------------------------------------------------------
! subroutine checkpoint_wr
!----------------------------------------------------------------------------

      subroutine checkpoint_wr_ncmpi_par (filenum, simtime)
!
! Do parallel i/o using Parallel netCDF
!
! MZ -- 2-20-00
! Jianwei -- 11/15/02
!
! This version of checkpoint uses Parallel netCDF to store the PARAMESH data.  The 
! IO is done in parallel -- no copying of the data to a single processor
! to do the writing is performed.  
!
! Parallel netCDF uses MPI-IO (via ROMIO) to support parallel IO.  Each processor
! must open the file, define the dataspaces for each netCDF variables.  
!
! A single record for each of the PARAMESH data structures is created.  A
! processor only writes to a subset of this record.  Each record has a 
! dimension with length = tot_blocks.  The offset of a processor into this 
! dimension is computed by looking at the total number of blocks that are
! below the current processor.
!
! In this version of the checkpoint, each variable is given its own 
! record -- this makes it easier to change the variable list in the
! future without disturbing the format of the file.
!
! The include file -- ncmpi_flash.h is used for the C routines and mirrors
! the necessary data from physicaldata.fh
!
! written for netCDF 3.0
!
!---------------------------------------------------------------------------

#include "common.fh"

      integer filenum
      real simtime
      
      integer block_no
      integer jproc, i, j, ivar 
      integer ngid 
      integer ierr

      integer n_to_left(0:16383)  ! must extend from 0 to NumPEs-1

! 2-20-00 -- we don't need to allocate more space than necessary
!      integer gid(mfaces+1+mchild,maxblocks_tr)
      integer gid(nfaces+1+nchild,maxblocks_tr)

      integer tot_blocks
      
      save gid, n_to_left

      integer nzones_block(3)

! create a character variable to hold the string representation of the block
! number.  Note this is set to be 4 characters long (i.e. max = 9999).  
      character*4  fnum_string
      character*80 filename, outstr

! create a temporary array to hold the 4 character variable names
! this will include those defined in definitions.fh and network_common.fh
      character*4 unklabels(nvar)

! storage for the current date and time
	character date_string*40

      character(len=4) :: ionam(ionmax), record_label
      
      integer file_id

      integer varid(6+nvar)

      integer global_offset

      character (len=40) :: flash_release
      real unk_buf(1,nxb,nyb,nzb,maxblocks)
      real time_start, time_io
      real :: coord_buf(ndim,lnblocks)
      real :: bs_buf(ndim,lnblocks)
      real :: bb_buf(2,ndim,lnblocks)


!-----------------------------------------------------------------------------
! compute the total number of blocks left of a given processor number
!-----------------------------------------------------------------------------
! use an allgather routine here 
      call MPI_Allgather(lnblocks, 1,MPI_INTEGER, & 
     &                   n_to_left,1,MPI_INTEGER, & 
     &                   MPI_COMM_WORLD,ierr)

      
! compute the total number of blocks
      tot_blocks = 0

      do i = 0,NumPEs-1
         tot_blocks = tot_blocks + n_to_left(i)
      end do
      
! compute the number of procssors to the left of a processor
      do i = NumPEs-1,1,-1
         n_to_left(i) = n_to_left(i-1)
      end do

      n_to_left(0) = 0
      do i = 2,NumPEs-1
         n_to_left(i) = n_to_left(i) + n_to_left(i-1)
      end do


!-----------------------------------------------------------------------------
! compute the global id -- this is a single array which stores the 
! neighbor block numbers, the parent, and the children of a given block
!-----------------------------------------------------------------------------
      do block_no = 1,lnblocks

         ngid = 0

! loop over the faces and store the neighbors
         do j = 1,nfaces
            ngid = ngid + 1

! if the neighbor exists, then store the block number of the neighbor
! -- take into account the number of blocks below the processor that the
! neighbor is on, so the block number is global
            if (neigh(1,j,block_no).gt.0) then
               gid(ngid,block_no) = neigh(1,j,block_no) +  & 
     &              n_to_left(neigh(2,j,block_no))
            else

! the neighbor is either a physical boundary or does not exist at that 
! level of refinement
               gid(ngid,block_no) = neigh(1,j,block_no)
            end if
         end do
        
! store the parent of the current block
         ngid = ngid + 1
         if (parent(1,block_no).gt.0) then
            gid(ngid,block_no) = parent(1,block_no) +  & 
     &           n_to_left(parent(2,block_no))
         else
            gid(ngid,block_no) = parent(1,block_no)
         end if
         
! store the children of the current block
         do j = 1,nchild
            ngid = ngid + 1
            if (child(1,j,block_no).gt.0) then
               gid(ngid,block_no) = child(1,j,block_no) +  & 
     &              n_to_left(child(2,j,block_no))
            else
               gid(ngid,block_no) = child(1,j,block_no)
            end if
         end do

      end do

!-----------------------------------------------------------------------------
! open the netCDF file
!-----------------------------------------------------------------------------
      write (fnum_string, '(i4.4)') filenum
      filename = trim(basenm) // 'ncmpi_chk_'// fnum_string

      call ncmpi_initialize_file(file_id, filename)


!-----------------------------------------------------------------------------
! store the scalar information -- # of blocks, simulation time, etc
!-----------------------------------------------------------------------------
      
! get the current time and date
	date_string = 'now'      

! store the number of zones / block in each direction
      nzones_block(1) = nxb
      nzones_block(2) = nyb
      nzones_block(3) = nzb

! get the names of the fluids being followed
      call get_mfluid_property ("short name", ionam)

! merge the two variable lists into one for storage
      unklabels(1:nvar-nuc2)     = varnam(:)
      unklabels(nvar-nuc2+1:nvar) = ionam(:)

#ifdef TIMERS
      time_start = MPI_Wtime()
#endif

      call ncmpi_write_header_info(MyPE, & 
     &                          nvar, & 
     &                          file_id, & 
     &                          date_string, &
     &                          flash_release(), &
     &                          tot_blocks, & 
     &                          simtime, & 
     &                          dt, & 
     &                          nstep, & 
     &                          nzones_block, & 
     &                          unklabels, &
     &                          varid)

#ifdef TIMERS
      print *, 'header: MyPE = ', MyPE, ' time = ',  & 
     &     MPI_Wtime() - time_start
#endif

      global_offset = n_to_left(MyPE)

!-----------------------------------------------------------------------------
! store the tree information
!-----------------------------------------------------------------------------

! store the refinement level
#ifdef TIMERS
      time_start = MPI_Wtime()
#endif

      call ncmpi_write_lrefine(file_id,  & 
     &                      varid(1), &
     &                      lrefine,  & 
     &                      lnblocks,  & 
     &                      tot_blocks,  & 
     &                      global_offset)

#ifdef TIMERS
      print *, 'lrefine: MyPE = ', MyPE, ' time = ',  & 
     &     MPI_Wtime() - time_start
#endif

! store the nodetype
#ifdef TIMERS
      time_start = MPI_Wtime()
#endif      

      call ncmpi_write_nodetype(file_id,  & 
     &                       varid(2), &
     &                       nodetype,  & 
     &                       lnblocks,  & 
     &                       tot_blocks,  & 
     &                       global_offset)

#ifdef TIMERS
      print *, 'nodetype: MyPE = ', MyPE, ' time = ',  & 
     &     MPI_Wtime() - time_start
#endif

! store the global id
#ifdef TIMERS
      time_start = MPI_Wtime()
#endif      

      call ncmpi_write_gid(file_id,  & 
     &                  varid(3), &
     &                  gid,  & 
     &                  lnblocks,  & 
     &                  tot_blocks,  & 
     &                  global_offset)

#ifdef TIMERS
      print *, 'gid: MyPE = ', MyPE, ' time = ',  & 
     &     MPI_Wtime() - time_start
#endif

!-----------------------------------------------------------------------------
! store the grid information
!-----------------------------------------------------------------------------
      
! store the coordinates
#ifdef TIMERS
      time_start = MPI_Wtime()
#endif
coord_buf(1:ndim,1:lnblocks) = coord(1:ndim,1:lnblocks)
      call ncmpi_write_coord(file_id,  & 
     &                    varid(4), &
     &                    coord_buf,  & 
     &                    lnblocks,  & 
     &                    tot_blocks,  & 
     &                    global_offset)
#ifdef TIMERS
      print *, 'coord: MyPE = ', MyPE, ' time = ',  & 
     &     MPI_Wtime() - time_start
#endif

! store the block size
#ifdef TIMERS
      time_start = MPI_Wtime()
#endif
bs_buf(1:ndim,1:lnblocks) = size(1:ndim,1:lnblocks)
      call ncmpi_write_size(file_id,  & 
     &                   varid(5), & 
     &                   bs_buf,  & 
     &                   lnblocks,  & 
     &                   tot_blocks,  & 
     &                   global_offset)

#ifdef TIMERS
      print *, 'size: MyPE = ', MyPE, ' time = ',  & 
     &     MPI_Wtime() - time_start
#endif

! store the bounding box 
#ifdef TIMERS
      time_start = MPI_Wtime()
#endif      
bb_buf(1:2,1:ndim,1:lnblocks) = bnd_box(1:2,1:ndim,1:lnblocks)
      call ncmpi_write_bnd_box(file_id,  & 
     &                      varid(6), & 
     &                      bnd_box, & 
     &                      lnblocks,  & 
     &                      tot_blocks,  & 
     &                      global_offset)

#ifdef TIMERS
      print *, 'bb1: MyPE = ', MyPE, ' time = ',  & 
     &     MPI_Wtime() - time_start
#endif


!-----------------------------------------------------------------------------
! store the unknowns -- here we will pass the entire unk array on each 
! processor.  The HDF 5 memory space functionality will pick just the 
! interior cells to write to disk.
!-----------------------------------------------------------------------------
      time_io = 0.e0

      do i = 1, nvar
         record_label = unklabels(i)

         unk_buf(1,1:nxb,1:nyb,1:nzb,:) = unk(i,nguard+1:nguard+nxb, & 
     &        nguard*k2d+1:nguard*k2d+nyb, & 
     &        nguard*k3d+1:nguard*k3d+nzb,:)

#ifdef TIMERS
         time_start = MPI_Wtime()
#endif

         call ncmpi_write_unknowns(file_id, & 
     &                          varid(6+i), & 
     &                          nxb, & 
     &                          nyb, & 
     &                          nzb, & 
     &                          unk_buf, & 
     &                          record_label,  & 
     &                          lnblocks,  & 
     &                          tot_blocks,  & 
     &                          global_offset)

#ifdef TIMERS
         time_io = time_io + (MPI_Wtime() - time_start)
#endif

      enddo

#ifdef TIMERS
      print *, 'unk: MyPE = ', MyPE, ' time = ', time_io
#endif

!-----------------------------------------------------------------------------
! close the file
!-----------------------------------------------------------------------------

      call ncmpi_close_file(file_id)

      if (MyPE .EQ. MasterPE) then

! write to stdout
         print *, '*** Wrote output to ', trim(filename),  & 
     &        ' (', tot_blocks, 'blocks ) ***'

! also store in the log file
         outstr = "*** wrote         blocks."
         write (outstr(11:17), '(I7)') tot_blocks

      end if

      return
      end




