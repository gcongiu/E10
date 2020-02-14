/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 *   Copyright (C) 2014-2016 Seagate Systems UK Ltd.
 *   See COPYRIGHT notice in top-level directory.
 *
 *   Author: Giuseppe Congiu <giuseppe.congiu@seagate.com>
 */
#ifndef __ADI_ATOMIC_QUEUE_H__
#define __ADI_ATOMIC_QUEUE_H__

#define _USE_PTHREAD_MUTEX_
/*
 * ADIOI_Atomic_queue - contains sync requests for specific thread
 *
 * - head_ is the head of the queue
 * - size_ is the size of the queue
 */
struct ADIOI_Atomic_queue {
    struct list_head head_;
    int size_;
#ifdef _USE_PTHREAD_MUTEX_
    pthread_mutex_t lock_;
    pthread_cond_t ready_;
#endif
};

/*
 * ADIOI_Sync_req - contains sync requests
 * - type_ is the type for the posix thread:
 *   ADIOI_THREAD_SYNC or ADIOI_THREAD_SHUTDOWN
 * - off_ is the offset of the sync request in the file
 * - datatype_ is the MPI datatype of the sync request
 * - count_ is the num of datatypes in the sync request
 * - req_ is the MPI_Request handle
 * - fflags_ flush flags for BEEGFS
 */
struct ADIOI_Sync_req {
    int type_;
    ADIO_Offset off_;
    MPI_Datatype datatype_;
    int count_;
    ADIO_Request *req_;
    int error_code_;
    int fflags_;
};

/*
 * ADIOI_Sync_req_env_t - envelop for sync requests
 *
 * The atomic queue contains envelops which point
 * to synchronisation requests.
 */
struct ADIOI_Sync_req_env {
    struct ADIOI_Sync_req *req_;
    struct list_head head_;
};
#endif
