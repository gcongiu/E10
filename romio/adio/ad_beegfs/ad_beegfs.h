/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 *   Copyright (C) 2014-2016 Seagate Systems UK Ltd.
 *   See COPYRIGHT notice in top-level directory.
 *
 *   Author: Giuseppe Congiu <giuseppe.congiu@seagate.com>
 */

#ifndef AD_BEEGFS_INCLUDE
#define AD_BEEGFS_INCLUDE

#include <unistd.h>
#include <sys/types.h>
#include <fcntl.h>
#include <stdbool.h>		/* needed by beegfs.h */
#include <beegfs/beegfs.h>	/* FHGFS_IOC & BEEGFS definitions */
#include <zlib.h>
#include "adio.h"
#include "adi_thread.h"
#include "atomic_queue.h"
#include "adi_atomic_queue.h"

/* Workaround for incomplete set of definitions if __REDIRECT is not
   defined and large file support is used in aio.h */
#if !defined(__REDIRECT) && defined(__USE_FILE_OFFSET64)
#define aiocb aiocb64
#endif

// this is needed to use SSD
// cache without native BeeGFS
// support
#define BEEGFS_UFS_CACHE_TEST

/* synchronisation routines */
int ADIOI_BEEGFS_Sync_thread_init(ADIOI_Sync_thread_t *t, ...);
int ADIOI_BEEGFS_Sync_thread_fini(ADIOI_Sync_thread_t *t);
void ADIOI_BEEGFS_Sync_thread_enqueue(ADIOI_Sync_thread_t t, ADIOI_Sync_req_t r);
void ADIOI_BEEGFS_Sync_thread_flush(ADIOI_Sync_thread_t t);
void ADIOI_BEEGFS_Sync_thread_wait(ADIOI_Sync_thread_t t);

void ADIOI_BEEGFS_Open(ADIO_File fd, int *error_code);
void ADIOI_BEEGFS_OpenColl(ADIO_File fd, int rank, int access_mode, int *error_code);
void ADIOI_BEEGFS_Close(ADIO_File fd, int *error_code);
void ADIOI_BEEGFS_Flush(ADIO_File fd, int *error_code);
void ADIOI_BEEGFS_WriteContig(ADIO_File fd, const void *buf, int count,
                              MPI_Datatype datatype, int file_ptr_type,
                              ADIO_Offset offset, ADIO_Status *status, int
                              *error_code);
void ADIOI_BEEGFS_SetInfo(ADIO_File fd, MPI_Info users_info, int *error_code);
#endif
