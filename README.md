#Exascale10 base code release v0.1

#Introduction

Exascale10 is part of the FP7 EU funded DEEP-ER project (Grant Agreement n. 610467).

Exascale10 contributes to the DEEP-ER project by providing improvements to existing 
collective I/O implementations. The ROMIO middleware is used as substrate into which 
the new DEEP-ER hardware enabled functionalities are included, maintaining a familiar, 
widely adopted, I/O interface, minimising the integration effort of the new features 
into existing and future applications.

An important advantage is the use of the NVMe devices integrated in the nodes of the 
DEEP-ER Prototype. This fast, persistent, cache layer amplifies collective I/O performance, 
and more generally, any I/O operation. The new memory tier in the DEEP-ER Prototype is made 
available to applications through the MPI-IO interface by means of additional hints, described 
in detail in the rest of this document. The new hints rely on the underlying Exascale10 code 
inside ROMIO to efficiently move data to and from the cache layer. Figure 1 shows the E10
software stack. The Exascale10 code comes in the form a ADIO plugin in the existing GEN layer
implementation, used by the UFS driver as well as other file system drivers.

                   +-----------------------------+
                   |                             |
                   |           MPIWRAP           |
                   |                             |
            ---    +-----------------------------+
             ^     |                             |
             |     |           MPI-IO            |
             |     |                             |
             |     +-----------------------------+
             |     |                             |
             |     |  ADIO (Abstract Device IO)  |
             |     |                             |
           ROMIO   +----------+----------+-------+
             |     |          |          |       |
             |     |  BEEGFS  |  Lustre  |  UFS  |
             |     |          |          |       |
             |     +----------+----------+-------+
             |     |             +------------+  |
             |     |  GEN layer  | E10 Plugin |  |
             v     |             +------------+  |
            ---    +-----------------------------+
                   |                             |
                   |          POSIX I/O          |
                   |                             |
                   +-----------------------------+
                 Figure 1: Exascale10 software stack


#Exascale10 hints extensions for MPI-IO

The Exascale10 hints extension for MPI-IO represents the only way users can access the DEEP-ER cache 
layer through MPI-IO. There is currently no additional API, although for the future it is planned to 
move the existing functionalities into a separate Exascale10 middleware, not relying on any other 
implementation. Follows a list of hints and corresponding description:

 * `e10_cache`:                 used to `enable` or `disable` the use of local NVM devices. When set to 
                                `coherent` the E10 implementation will try to acquire a lock for every file 
                                extent in the local file system cache and will release them only when the 
                                cache file is in sync with the global file. Default is `enable`.
 * `e10_cache_path`:            used to define the pathname of the cache file in the local file system.
 * `e10_cache_flush_flag`:      used to define when the cache synchronisation should start. `flush_immediate`
                                will trigger cache synchronsiation immediately after the write operation
                                returns. `flush_onclose` will start synchronisation when the file is closed.
                                Default is `flush_immediate`.
 * `e10_cache_discard_flag`:    used to `enable` or `disable` cache file deletion from the local file system 
                                when the file is closed. Default is `enable`
 * `e10_cache_threads`:         used to define the number of threads in the synchronisation pool. Default is
                                `1` thread.


#Provided packages

The Exascale10 distribution contains the following packages:

 * romio package containing the core E10 implementation
 * mpiwrap package, a wrapper library for MPI-IO that enables transparent MPI-IO hints manipulation
 * mpe2 package, a profiling library for MPI
 * benchmark package containing different collective I/O benchmarks like coll_perf, IOR and Flash-IO
 * script package containing utility scripts to generate batch scripts and to process mpe log files


#ROMIO

The Exascale10 modifications to the ROMIO code are mainly targeted to the “common” implementation (i.e. adio/common).
Nevertheless, an additional ADIO driver supporting the BeeGFS file system from Fraunhofer ITMW has also been 
implemented.

The base code of ROMIO has been extended to provide all the mechanisms supporting the new MPI-IO hints. For example, 
the `MPI_File` descriptor of type `struct ADIO_FileD` has been extended with an additional `cache_fd` of the same type
to contain the file handle of the local cache file. When the file is open with `MPI_File_open()` the new implementation
will try to open an additional local file to contain cache data. If the local open fails the standard open is performed.

The just described mechanism is implemented in the `adio/common/ad_opencoll.c` file. Furthermore, a synchronisation 
thread pool is also started to take care of the cache synchronisation to the global file. Each thread in the pool 
is of type `ADIOI_Sync_thread_t`, defined in `adio/include/adi_thread.h`. This data structure provides different APIs to 
the main thread as well as the synchronisation thread (described in more detail later in the document).

The write routine in ROMIO has also been modified to redirect data to the cache_fd and to create and submit sync 
requests to the thread pool. Synchronisation requests are of type `ADIOI_Sync_req_t`. This data structure also provides
a set of dedicated APIs to, e.g., initialise and finalise synchronisation requests.

The MPI-IO close and sync routines have been modified to flush the cache file to the global file and wait for flush 
to complete. `MPI_File_close()` for example will now start `ADIO_GEN_Flush()` which will internally use the sync thread
APIs to flush the cache and wait for completion.


##`ADIOI_Sync_thread_t` APIs (`adio/common/adi_cache_sync.c`)

* `ADIOI_Sync_thread_init(ADIOI_Sync_thread_t *t, ...)`: initialise a new sync thread. The new thread contains three queues.
  A pending queue (`pen_`) which receives `ADIOI_Sync_req_t`(s) from the main thread and buffers them, a submitted queue (`sub_`)
  that contains requests that have been flushed and are thus ready to be satisfied and a wait queue (`wait_`) that contains
  a copy of the pointer of the `ADIOI_Sync_req_t`(s) that have been inserted in the sub_ queue. This queue is used to check 
  upon the completion of requests when `MPI_Wait()` is invoked.
  The init routine also starts a posix thread passing to it a pointer to the `ADIOI_Sync_thread_start()` routine. This will 
  continuously check for new sync requests in the `sub_` queue.

* `ADIOI_Sync_thread_fini(ADIOI_Sync_thread_t *t)`: finilise a synchronisation thread. The finalisation process consists in
  the creation and submission to the `pen_` queue of a special type of `ADIOI_Sync_req_t` that will shutdown the posix thread.

* `ADIOI_Sync_thread_enqueue(ADIOI_Sync_thread_t t, ADIOI_Sync_req_t r)`: add a new request to the `pen_` queue.

* `ADIOI_Sync_thread_flush(ADIOI_Sync_thread_t t, ADIOI_Sync_req_t r)`: move all the sync request in the `pen_` queue to the 
  `sub_` queue.

* `ADIOI_Sync_thread_wait(ADIOI_Sync_thread_t t)`: wait for all the requests in the `sub_` queue to be completed. This is done
  by `MPI_Wait()` on every request in the `wait_` queue.


##`ADIOI_Sync_req_t` APIs (`adio/common/adi_atomic_queue.c`)

* `ADIOI_Sync_req_init(ADIOI_Sync_req_t *r, ...)`: initialise a new sync request. Request can be of type `ADIOI_THREAD_SYNC`
  or `ADIOI_THREAD_SHUTDOWN` to sync the cache or shut the thread down respectively.

* `ADIOI_Sync_req_fini(ADIOI_Sync_req_t *r)`: finalise a sync request.

* `ADIOI_Sync_req_set(ADIOI_Sync_req_t r, ...)`: set a property of the sync request.

* `ADIOI_Sync_req_get(ADIOI_Sync_req_t r, ...)`: get a property of the sync request.


#MPIWRAP (`mpiwrap/mpiwrap.cpp`)

MPIWRAP is a wrapper library for MPI-IO. MPIWRAP can be used to pass the E10 hints to the application transparently. Hints
can be defined in a configuration file in the Json format. Follow an example of configuration file:

      {
          “Guardnames”: “test_file”,
          “File”: [{
              “Path”: “/work/deep47/test_file”,
              “Type”: “MPI”,
              “cb_buffer_size”: “4194304”,
              “ind_wr_buffer_size”: “524288”,
              “cb_nodes”: “64”,
              “romio_cb_read”: “enable”,
              “romio_cb_write”: “enable”,
              “romio_no_indep_rw”: “true”,
              “striping_unit”: “4194304”,
              “striping_factor”: “4”,
              “e10_cache”: “enable”,
              “e10_cache_path”: “/scratch”,
              “e10_cache_flush_flag”: “flush_immediate”,
              “e10_cache_discard_flag”: “enable”,
              “e10_cache_threads”: “1”
          }]
      }

* Guardnames: is a special type of field. It is used to delay the `MPI_File_close()` operation inside MPIWRAP. This is required
              whenever the `e10_cache` hint is set to `enable` and the `e10_cache_flush_flag` is set to `flush_immediate`. In
              this case `MPI_File_write_all()` will return as soon as data has been written to the local cache file. Meanwhile
              synchronisation is started by the thread pool. This requires a valid file handle to move the data thus the file
              cannot be closed. Instead the wrapped `MPI_File_close()` will return `MPI_SUCCESS` without closing the file. The file
              will be closed only when the next shared file is opened (before) in the wrapped `MPI_File_open()`. The Guardnames 
              field contains the base name of the shared files written by the application. Performing this open/close modification
              behind the scenes MPIWRAP can make sure cache synchronisation is overlapped with computation.

* Path:       is the name of the file to which hints will be applied. This field is interpreted in a flexible way by MPIWRAP. In
              particular, it is used to match all the files that have the defined name as well as all the file having the same
              name suffix or all the files that reside in the directory pathname.

* Type:       is always MPI
