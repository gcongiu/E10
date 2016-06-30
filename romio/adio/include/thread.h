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

typedef struct ADIOI_Sync_thread *ADIOI_Sync_thread_t;

int ADIOI_Sync_thread_init(ADIOI_Sync_thread_t *, ...);
int ADIOI_Sync_thread_fini(ADIOI_Sync_thread_t *);
void ADIOI_Sync_thread_enqueue(ADIOI_Sync_thread_t, ADIOI_Sync_req_t);
void ADIOI_Sync_thread_flush(ADIOI_Sync_thread_t);
void ADIOI_Sync_thread_wait(ADIOI_Sync_thread_t);
#endif
