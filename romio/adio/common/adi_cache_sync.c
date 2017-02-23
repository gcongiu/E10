/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 *   Copyright (C) 2014-2016 Seagate Systems UK Ltd.
 *   See COPYRIGHT notice in top-level directory.
 *
 *   Author: Giuseppe Congiu <giuseppe.congiu@seagate.com>
 */
#include "adio.h"
#include "adio_extern.h"
#include "thread.h"
#include "adi_thread.h"
#include "atomic_queue.h"
#include <assert.h>

/* local functions prototypes */
static void *ADIOI_Sync_thread_start(void *);
static int ADIOI_Sync_req_query(void *extra_state, MPI_Status *status);
static int ADIOI_Sync_req_free(void *extra_state);
static int ADIOI_Sync_req_cancel(void *extra_state, int complete);

/*
 * ADIOI_Sync_thread_init - initialise synchronisation thread
 */
int ADIOI_Sync_thread_init(ADIOI_Sync_thread_t *t, ...) {
    int rc, error_code = MPI_SUCCESS;
    va_list args;

    *t = (struct ADIOI_Sync_thread *)ADIOI_Malloc(sizeof(struct ADIOI_Sync_thread));
    pthread_attr_init(&((*t)->attr_));
    pthread_attr_setdetachstate(&((*t)->attr_), PTHREAD_CREATE_JOINABLE);

    /* get thread params */
    va_start(args, t);
    (*t)->fd_ = va_arg(args, ADIO_File);
    va_end(args);

    /* create queues */
    ADIOI_Atomic_queue_init(&((*t)->sub_));
    ADIOI_Atomic_queue_init(&((*t)->pen_));
    ADIOI_Atomic_queue_init(&((*t)->wait_));

    /* create thread */
    rc = pthread_create(&((*t)->tid_), &((*t)->attr_), ADIOI_Sync_thread_start, *t);

    if (rc)
	error_code = ADIOI_ERR_THREAD_CREATE;

    return error_code;
}

/*
 * ADIOI_Sync_thread_fini - finalise synchronisation thread
 */
int ADIOI_Sync_thread_fini(ADIOI_Sync_thread_t *t) {
    ADIOI_Sync_req_t fin;

    /* init fin request */
    ADIOI_Sync_req_init(&fin, ADIOI_THREAD_SHUTDOWN);

    /* submit fin request */
    ADIOI_Atomic_queue_push((*t)->sub_, fin);

    /* join the sync thread */
    pthread_join((*t)->tid_, NULL);

    /* fini fin request */
    ADIOI_Sync_req_fini(&fin);

    /* fini queues */
    ADIOI_Atomic_queue_fini(&((*t)->sub_));
    ADIOI_Atomic_queue_fini(&((*t)->pen_));
    ADIOI_Atomic_queue_fini(&((*t)->wait_));

    ADIOI_Free(*t);

    return MPI_SUCCESS;
}

/*
 * ADIOI_Sync_thread_enqueue - enqueue new syn request for thread
 */
void ADIOI_Sync_thread_enqueue(ADIOI_Sync_thread_t t, ADIOI_Sync_req_t r) {
    ADIOI_Atomic_queue_push(t->pen_, r);
}

/*
 * ADIOI_Sync_thread_flush - flush pending requests for thread
 */
void ADIOI_Sync_thread_flush(ADIOI_Sync_thread_t t) {
    ADIOI_Sync_req_t r;
    ADIO_Request *req;

    while (!ADIOI_Atomic_queue_empty(t->pen_)) {
	r = ADIOI_Atomic_queue_front(t->pen_);
	ADIOI_Atomic_queue_pop(t->pen_);

	ADIOI_Sync_req_get_key(r, ADIOI_SYNC_REQ, &req);

	/* start Grequest */
	MPI_Grequest_start(&ADIOI_Sync_req_query,
			   &ADIOI_Sync_req_free,
			   &ADIOI_Sync_req_cancel,
			   (void *)r, req);

	/* push to the submission queue */
	ADIOI_Atomic_queue_push(t->sub_, r);

	/* push to wait queue */
	ADIOI_Atomic_queue_push(t->wait_, r);
    }
}

/*
 * ADIOI_Sync_thread_wait - wait for ongoing request to complete
 */
void ADIOI_Sync_thread_wait(ADIOI_Sync_thread_t t) {
    ADIOI_Sync_req_t wait, last;
    MPI_Status status;
    ADIO_Request *req;

    if (!ADIOI_Atomic_queue_empty(t->wait_)) {
	last = ADIOI_Atomic_queue_back(t->wait_);
	ADIOI_Sync_req_get_key(last, ADIOI_SYNC_REQ, &req);
	MPI_Wait(req, &status);
    }

    while (!ADIOI_Atomic_queue_empty(t->wait_)) {
	wait = ADIOI_Atomic_queue_front(t->wait_);
	ADIOI_Sync_req_get_key(wait, ADIOI_SYNC_REQ, &req);
	ADIOI_Atomic_queue_pop(t->wait_);
	ADIOI_Sync_req_fini(&wait);
	ADIOI_Free(req);
    }
}

/*
 * ADIOI_Sync_thread_pool_init - initialise synchronisation thread pool
 */
int ADIOI_Sync_thread_pool_init(ADIO_File fd, ...) {
    int rc, i, error_code, nt;
    char myname[] = "ADIOI_SYNC_THREAD_POOL_INIT";
    va_list args;

    error_code = MPI_SUCCESS;
    nt = fd->hints->e10_cache_threads;

    /* allocate memory for thread pool */
    fd->thread_pool = (ADIOI_Sync_thread_t *)
	ADIOI_Malloc(nt*sizeof(ADIOI_Sync_thread_t));

    /* init index for round robin distribution */
    fd->thread_curr = 0;

    /* init threads in the pool */
    for (i = 0; i < nt; i++) {
	error_code = ADIOI_Sync_thread_init(&(fd->thread_pool[i]), fd);

	/* error handling */
	if (error_code) {
	    fd->hints->e10_cache_threads = i;
	    break;
	}
    }

    return error_code;
}

/*
 * ADIOI_Sync_thread_pool_fini - finalise synchronisation thread pool
 *
 *       ADIOI_xxx_Flush()
 *              |
 *              v
 * ADIOI_Sync_thread_pool_fini()
 */
int ADIOI_Sync_thread_pool_fini(ADIO_File fd) {
    int i, nt;
    char myname[] = "ADIOI_SYNC_THREAD_POOL_FINI";

    /* just in case */
    if (fd->thread_pool == NULL)
	goto fn_exit;

    nt = fd->hints->e10_cache_threads;

    /* only shutdown running threads */
    for (i = 0; i < nt; i++) {
	ADIOI_Sync_thread_fini(&(fd->thread_pool[i]));
    }

    /* clean up memory */
    ADIOI_Free(fd->thread_pool);

    /* reset to null */
    fd->thread_pool = NULL;

fn_exit:
    return MPI_SUCCESS;
}

/*
 * ADIOI_Sync_thread_start - start the synchronisation routine
 */
void *ADIOI_Sync_thread_start(void *ptr) {
    ADIOI_Sync_thread_t t = (ADIOI_Sync_thread_t)ptr;
    ADIOI_Atomic_queue_t q = (ADIOI_Atomic_queue_t)t->sub_;
    ADIOI_Sync_req_t r;
    size_t wr_count;
    MPI_Count datatype_size;
    char *buf;
    ADIO_Offset bytes_xfered, len, buf_size, offset, off;
    int type, count, fflags, error_code;
    ADIO_Request *req;
    MPI_Datatype datatype;

    /* get sync buffer size */
    t->fd_;
    buf_size = t->fd_->hints->ind_wr_buffer_size;
    buf = (char *)ADIOI_Malloc(buf_size);

    for(;;) {
	/* get a new sync request */
#ifndef _USE_PTHREAD_MUTEX_
	if ((r = ADIOI_Atomic_queue_front(q)) == NULL)
	    continue;
#else
	r = ADIOI_Atomic_queue_front(q);
#endif
	/* pop sync request */
	ADIOI_Atomic_queue_pop(q);

	/* get request type */
	ADIOI_Sync_req_get_key(r, ADIOI_SYNC_TYPE, &type);

	/* check for shutdown type */
	if (type == ADIOI_THREAD_SHUTDOWN) {
	    break;
	}

	/* if sync type get all the fields */
	ADIOI_Sync_req_get_key(r, ADIOI_SYNC_ALL, &offset,
		&datatype, &count, &req, &error_code, &fflags);

	/* init I/O req */
	MPI_Type_size_x(datatype, &datatype_size);
	len = (ADIO_Offset)datatype_size * (ADIO_Offset)count;
	bytes_xfered = 0;
	off = offset;

	/* satisfy sync req */
	while (bytes_xfered < len) {
	    wr_count = (size_t)ADIOI_MIN(buf_size, len - bytes_xfered);
#ifdef ADIOI_MPE_LOGGING
	    MPE_Log_event(ADIOI_MPE_thread_read_a, 0, NULL);
#endif
	    /* read data from cache file */
	    pread(t->fd_->cache_fd->fd_sys, buf, wr_count, offset);
#ifdef ADIOI_MPE_LOGGING
	    MPE_Log_event(ADIOI_MPE_thread_read_b, 0, NULL);
	    MPE_Log_event(ADIOI_MPE_thread_write_a, 0, NULL);
#endif
	    /* write data to global file */
	    pwrite(t->fd_->fd_sys, buf, wr_count, offset);
#ifdef ADIOI_MPE_LOGGING
	    MPE_Log_event(ADIOI_MPE_thread_write_b, 0, NULL);
#endif
	    /* update offset */
	    bytes_xfered += (ADIO_Offset)wr_count;
	    offset += (ADIO_Offset)wr_count;
	}

	/* unlock extent locked in ADIO_WriteContig() */
	if (t->fd_->hints->e10_cache_coherent == ADIOI_HINT_ENABLE)
	    ADIOI_UNLOCK(t->fd_, off, SEEK_SET, len);

	/*  ---Begin Error Handling--- */
	/*  --- End Error Handling --- */

	/* complete Grequest */
	MPI_Grequest_complete(*req);
    }

    ADIOI_Free(buf);
    pthread_exit(NULL);
}

/*
 * ADIOI_Sync_req_query - MPI_Grequest query callback function
 */
int ADIOI_Sync_req_query(void *extra_state, MPI_Status *status) {
    ADIOI_Sync_req_t r = (ADIOI_Sync_req_t)extra_state;
    int count;
    int error_code;
    MPI_Datatype datatype;

    ADIOI_Sync_req_get_key(r, ADIOI_SYNC_ERR_CODE, &error_code);
    ADIOI_Sync_req_get_key(r, ADIOI_SYNC_DATATYPE, &datatype);
    ADIOI_Sync_req_get_key(r, ADIOI_SYNC_COUNT, &count);

    MPI_Status_set_cancelled(status, 0);

    if(error_code == MPI_SUCCESS)
        MPI_Status_set_elements(status, datatype, count);

    status->MPI_SOURCE = MPI_UNDEFINED;
    status->MPI_TAG = MPI_UNDEFINED;

    return error_code;
}

/*
 * ADIOI_Sync_req_free - MPI_Grequest free callback function
 */
int ADIOI_Sync_req_free(void *extra_state) {
    return MPI_SUCCESS;
}

/*
 * ADIOI_Sync_req_cancel - MPI_Grequest cancel callback function
 */
int ADIOI_Sync_req_cancel(void *extra_state, int complete) {
    return MPI_SUCCESS;
}

/*
 * vim: ts=8 sts=4 sw=4 noexpandtab
 */
