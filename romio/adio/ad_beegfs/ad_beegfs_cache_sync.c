/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 *   Copyright (C) 2014-2016 Seagate Systems UK Ltd.
 *   See COPYRIGHT notice in top-level directory.
 *
 *   Author: Giuseppe Congiu <giuseppe.congiu@seagate.com>
 */

#include "adio.h"
#include "adio_extern.h"
#include "ad_beegfs.h"
#include <assert.h>

#ifdef AGGREGATION_PROFILE
#include "mpe.h"
#endif

/*
 * ADIOI_BEEGFS_Sync_thread_init -
 */
int ADIOI_BEEGFS_Sync_thread_init(ADIOI_Sync_thread_t *t, ...) {
    va_list args;
    *t = (struct ADIOI_Sync_thread *)ADIOI_Malloc(sizeof(struct ADIOI_Sync_thread));

    /* get thread params */
    va_start(args, t);
    (*t)->fd_ = va_arg(args, ADIO_File);
    va_end(args);

    /* create queues */
    ADIOI_Atomic_queue_init(&((*t)->sub_));
    ADIOI_Atomic_queue_init(&((*t)->pen_));
    ADIOI_Atomic_queue_init(&((*t)->wait_));

    return MPI_SUCCESS;
}

/*
 * ADIOI_BEEGFS_Sync_thread_fini -
 */
int ADIOI_BEEGFS_Sync_thread_fini(ADIOI_Sync_thread_t *t) {
    ADIOI_Atomic_queue_fini(&((*t)->sub_));
    ADIOI_Atomic_queue_fini(&((*t)->pen_));
    ADIOI_Atomic_queue_fini(&((*t)->wait_));
    ADIOI_Free(*t);
    *t = NULL;

    return MPI_SUCCESS;
}

/*
 * ADIOI_BEEGFS_Sync_thread_enqueue -
 */
void ADIOI_BEEGFS_Sync_thread_enqueue(ADIOI_Sync_thread_t t, ADIOI_Sync_req_t r) {
    ADIOI_Sync_thread_enqueue(t, r);
}

/*
 * ADIOI_BEEGFS_Sync_thread_flush -
 */
void ADIOI_BEEGFS_Sync_thread_flush(ADIOI_Sync_thread_t t) {
    ADIOI_Sync_req_t r;

    while (!ADIOI_Atomic_queue_empty(t->pen_)) {
	r = ADIOI_Atomic_queue_front(t->pen_);
	ADIOI_Atomic_queue_pop(t->pen_);
	ADIOI_Atomic_queue_push(t->sub_, r);
	ADIOI_BEEGFS_Sync_thread_start(t);

	/* push wait request to wait queue */
	ADIOI_Atomic_queue_push(t->wait_, r);
    }
}

/*
 * ADIOI_BEEGFS_Sync_thread_wait -
 */
void ADIOI_BEEGFS_Sync_thread_wait(ADIOI_Sync_thread_t t) {
    ADIOI_Sync_req_t wait;
    MPI_Status status;
    ADIO_Request *req;
    int myrank;

    /* wait for completion using deeper_cache_flush_wait() */
    if (deeper_cache_flush_wait(t->fd_->filename, DEEPER_FLUSH_NONE)) {
	MPI_Comm_rank(t->fd_->comm, &myrank);
	FPRINTF(stderr, "rank = %d -> deeper_cache_flush_wait(%s, DEEPER_FLUSH_NONE) = %s\n",
		myrank, t->fd_->filename, strerror(errno));
    }

    /* empty wait queue and free MPI_Requests */
    while (!ADIOI_Atomic_queue_empty(t->wait_)) {
	wait = ADIOI_Atomic_queue_front(t->wait_);
	if (t->fd_->hints->e10_cache_coherent == ADIOI_HINT_ENABLE) {
	    int count;
	    ADIO_Offset offset, len;
	    MPI_Datatype datatype;
	    MPI_Count datatype_size;

	    ADIOI_Sync_req_get_key(wait, ADIOI_SYNC_OFFSET, &offset);
	    ADIOI_Sync_req_get_key(wait, ADIOI_SYNC_DATATYPE, &datatype);
	    ADIOI_Sync_req_get_key(wait, ADIOI_SYNC_COUNT, &count);  

	    MPI_Type_size_x(datatype, &datatype_size);
	    len = (ADIO_Offset)datatype_size * (ADIO_Offset)count;

	    ADIOI_UNLOCK(t->fd_, offset, SEEK_SET, len);
	}
	ADIOI_Sync_req_get_key(wait, ADIOI_SYNC_REQ, &req);
	ADIOI_Atomic_queue_pop(t->wait_);
	ADIOI_Sync_req_fini(&wait);
	ADIOI_Free(req);
    }
}

/*
 * ADIOI_BEEGFS_Sync_thread_start - start synchronisation of req
 */
int ADIOI_BEEGFS_Sync_thread_start(ADIOI_Sync_thread_t t) {
    ADIOI_Atomic_queue_t q = t->sub_;
    ADIOI_Sync_req_t r;
    int retval, count, fflags, error_code, i;
    ADIO_Offset offset, len;
    MPI_Count datatype_size;
    MPI_Datatype datatype;
    ADIO_Request *req;
    char myname[] = "ADIOI_BEEGFS_SYNC_THREAD_START";

    r = ADIOI_Atomic_queue_front(q);
    ADIOI_Atomic_queue_pop(q);

    ADIOI_Sync_req_get_key(r, ADIOI_SYNC_ALL, &offset,
	    &datatype, &count, &req, &error_code, &fflags);

    MPI_Type_size_x(datatype, &datatype_size);
    len = (ADIO_Offset)datatype_size * (ADIO_Offset)count;

    retval = deeper_cache_flush_range(t->fd_->filename, (off_t)offset, (size_t)len, fflags);
/*
    FPRINTF(stderr, "deeper_cache_flush_range(%s, %lli, %llu, %d) = %d\n",
	   t->fd_->filename, offset, len, fflags, retval);
*/
    if (retval == DEEPER_RETVAL_ERROR) {
	FPRINTF(stderr, "%s: error %s\n", __func__, strerror(errno));

	return MPIO_Err_create_code(MPI_SUCCESS,
		                    MPIR_ERR_RECOVERABLE,
		                    "ADIOI_BEEGFS_Cache_sync_req",
			            __LINE__, MPI_ERR_IO, "**io %s",
			            strerror(errno));

    }
    
    return MPI_SUCCESS;
}
/*
 * vim: ts=8 sts=4 sw=4 noexpandtab
 */
