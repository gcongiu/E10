/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 *   Copyright (C) 2014-2016 Seagate Systems UK Ltd.
 *   See COPYRIGHT notice in top-level directory.
 *
 *   Author: Giuseppe Congiu <giuseppe.congiu@seagate.com>
 *
 *   Contains internal synchronisation thread definition
 */
#ifndef __ADI_THREAD_H__
#define __ADI_THREAD_H__

/*
 * ADIOI_Sync_thread - contains sync thread context
 *
 * - fd_ is the MPI file handle
 * - tid_ is the posix thread handle
 * - attr_ is the posix thread attributes
 * - sub_ is the queue containing submitted sync req
 * - pen_ is the queue containing pending sync req
 * - wait_ is the queue containing waiting sync requ
 */
struct ADIOI_Sync_thread {
    ADIO_File fd_;
    pthread_t tid_;
    pthread_attr_t attr_;
    ADIOI_Atomic_queue_t sub_;
    ADIOI_Atomic_queue_t pen_;
    ADIOI_Atomic_queue_t wait_;
};
#endif
