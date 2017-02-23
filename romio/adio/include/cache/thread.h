/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 *   Copyright (C) 2014-2016 Seagate Systems UK Ltd.
 *   See COPYRIGHT notice in top-level directory.
 *
 *   Author: Giuseppe Congiu <giuseppe.congiu@seagate.com>
 */
#ifndef __THREAD_H__
#define __THREAD_H__

#include <pthread.h>
#include "atomic_queue.h"

#define ADIOI_ERR_THREAD_CREATE 80 /* sync thread create err */

/* Cache allocation function */
void ADIOI_Cache_alloc(ADIO_File fd, ADIO_Offset off, ADIO_Offset len, int *error_code);

/* Available commands for sync thread */
enum {
    ADIOI_THREAD_SYNC     = 0, /* sync request */
    ADIOI_THREAD_SHUTDOWN = 1  /* shutdown request */
    /* BEEGFS HINTS */
    ADIOI_HINT_FLUSHIMMEDIATE = 4,
    ADIOI_HINT_FLUSHONCLOSE   = 8,
    ADIOI_HINT_FLUSHNONE      = 16
};

typedef struct ADIOI_Sync_thread *ADIOI_Sync_thread_t;

int ADIOI_Sync_thread_init(ADIOI_Sync_thread_t *, ...);
int ADIOI_Sync_thread_fini(ADIOI_Sync_thread_t *);
void ADIOI_Sync_thread_enqueue(ADIOI_Sync_thread_t, ADIOI_Sync_req_t);
void ADIOI_Sync_thread_flush(ADIOI_Sync_thread_t);
void ADIOI_Sync_thread_wait(ADIOI_Sync_thread_t);
#endif
