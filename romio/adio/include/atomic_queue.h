/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 *   Copyright (C) 2014-2016 Seagate Systems UK Ltd.
 *   See COPYRIGHT notice in top-level directory.
 *
 *   Author: Giuseppe Congiu <giuseppe.congiu@seagate.com>
 */
#ifndef __ATOMIC_QUEUE_H__
#define __ATOMIC_QUEUE_H__

/* for ADIOI_Sync_req_get_key() */
enum {
	ADIOI_SYNC_TYPE = 0,
	ADIOI_SYNC_OFFSET,
	ADIOI_SYNC_DATATYPE,
	ADIOI_SYNC_COUNT,
	ADIOI_SYNC_REQ,
	ADIOI_SYNC_ERR_CODE,
	ADIOI_SYNC_FFLAGS,
	ADIOI_SYNC_ALL,
	ADIOI_SYNC_REQ_ERR
};

/* Synchronisation Request Interfaces */
typedef struct ADIOI_Sync_req *ADIOI_Sync_req_t;

int ADIOI_Sync_req_init(ADIOI_Sync_req_t *, ...);
int ADIOI_Sync_req_init_from(ADIOI_Sync_req_t *, ADIOI_Sync_req_t);
int ADIOI_Sync_req_fini(ADIOI_Sync_req_t *);
int ADIOI_Sync_req_get_key(ADIOI_Sync_req_t, ...);
int ADIOI_Sync_req_set_key(ADIOI_Sync_req_t, ...);

/* Atomic Queue Interfaces */
typedef struct ADIOI_Atomic_queue *ADIOI_Atomic_queue_t;

void ADIOI_Atomic_queue_init(ADIOI_Atomic_queue_t *);
void ADIOI_Atomic_queue_fini(ADIOI_Atomic_queue_t *);
int ADIOI_Atomic_queue_empty(ADIOI_Atomic_queue_t);
int ADIOI_Atomic_queue_size(ADIOI_Atomic_queue_t);
ADIOI_Sync_req_t ADIOI_Atomic_queue_front(ADIOI_Atomic_queue_t);
ADIOI_Sync_req_t ADIOI_Atomic_queue_back(ADIOI_Atomic_queue_t);
void ADIOI_Atomic_queue_push(ADIOI_Atomic_queue_t, ADIOI_Sync_req_t);
void ADIOI_Atomic_queue_pop(ADIOI_Atomic_queue_t);
#endif
