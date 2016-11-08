/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 *   Copyright (C) 2014-2016 Seagate Systems UK Ltd.
 *   See COPYRIGHT notice in top-level directory.
 *
 *   Author: Giuseppe Congiu <giuseppe.congiu@seagate.com>
 */

#include "adio.h"
#include "adio_extern.h"
#include "mpiu_greq.h"
#include "ad_beegfs.h"
#include <assert.h>

#ifdef AGGREGATION_PROFILE
#include "mpe.h"
#endif

/*
 * ADIOI_BEEGFS_Sync_thread_init -
 */
int ADIOI_BEEGFS_Sync_thread_init(ADIOI_Sync_thread_t *t, ...) {

    int item;
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

    ADIOI_Sync_thread_wait(t);
}

static MPIX_Grequest_class ADIOI_BEEGFS_greq_class = 0;

int ADIOI_BEEGFS_Sync_req_poll(void *extra_state, MPI_Status *status);
int ADIOI_BEEGFS_Sync_req_wait(int count, void **array_of_states, double timeout, MPI_Status *status);
int ADIOI_BEEGFS_Sync_req_query(void *extra_state, MPI_Status *status);
int ADIOI_BEEGFS_Sync_req_free(void *extra_state);
int ADIOI_BEEGFS_Sync_req_cancel(void *extra_state, int complete);

/* arguments for callback function */
struct callback {
    ADIO_File fd_;
    ADIOI_Sync_req_t req_;
};

/*
 * ADIOI_BEEGFS_Sync_thread_start - start synchronisation of req
 */
int ADIOI_BEEGFS_Sync_thread_start(ADIOI_Sync_thread_t t) {

    ADIOI_Atomic_queue_t q = t->sub_;
    ADIOI_Sync_req_t r;
    int retval, count, fflags, error_code;
    ADIO_Offset offset, len;
    MPI_Aint lb, extent;
    MPI_Datatype datatype;
    ADIO_Request *req;
    char myname[] = "ADIOI_BEEGFS_SYNC_THREAD_START";

    r = ADIOI_Atomic_queue_front(q);
    ADIOI_Atomic_queue_pop(q);

    ADIOI_Sync_req_get_key(r, ADIOI_SYNC_ALL, &offset,
	    &datatype, &count, &req, &error_code, &fflags);

    MPI_Type_get_extent(datatype, &lb, &extent );
    len = (ADIO_Offset)extent * (ADIO_Offset)count;

    retval = deeper_cache_flush_range(t->fd_->filename, (off_t)offset, (size_t)len, fflags);

    if (retval == DEEPER_RETVAL_SUCCESS && ADIOI_BEEGFS_greq_class == 0) {
        MPIX_Grequest_class_create(ADIOI_BEEGFS_Sync_req_query,
                                   ADIOI_BEEGFS_Sync_req_free,
                                   MPIU_Greq_cancel_fn,
                                   ADIOI_BEEGFS_Sync_req_poll,
                                   ADIOI_BEEGFS_Sync_req_wait,
                                   &ADIOI_BEEGFS_greq_class);
    }
    else {
        /* --BEGIN ERROR HANDLING-- */
        return MPIO_Err_create_code(MPI_SUCCESS,
                                    MPIR_ERR_RECOVERABLE,
                                    "ADIOI_BEEGFS_Cache_sync_req",
                                    __LINE__, MPI_ERR_IO, "**io %s",
                                    strerror(errno));
        /* --END ERROR HANDLING-- */
    }

    /* init args for the callback functions */
    struct callback *args = (struct callback *)ADIOI_Malloc(sizeof(struct callback));
    args->fd_ = t->fd_;
    args->req_ = r;

    MPIX_Grequest_class_allocate(ADIOI_BEEGFS_greq_class, args, req);

    return MPI_SUCCESS;
}

/*
 * ADIOI_BEEGFS_Sync_req_poll -
 */
int ADIOI_BEEGFS_Sync_req_poll(void *extra_state, MPI_Status *status) {

    struct callback *cb = (struct callback *)extra_state;
    ADIOI_Sync_req_t r = (ADIOI_Sync_req_t)cb->req_;
    ADIO_File fd = (ADIO_File)cb->fd_;
    char *filename = fd->filename;
    int count, cache_flush_flags, error_code;
    MPI_Datatype datatype;
    ADIO_Offset offset;
    MPI_Aint lb, extent;
    ADIO_Offset len;
    ADIO_Request *req;

    ADIOI_Sync_req_get_key(r, ADIOI_SYNC_ALL, &offset,
	    &datatype, &count, &req, &error_code, &cache_flush_flags);

    int retval = deeper_cache_flush_wait(filename, cache_flush_flags);

    MPI_Type_get_extent(datatype, &lb, &extent);
    len = (ADIO_Offset)extent * (ADIO_Offset)count;

    if (fd->hints->e10_cache_coherent == ADIOI_HINT_ENABLE)
	ADIOI_UNLOCK(fd, offset, SEEK_SET, len);

    /* mark generilized request as completed */
    MPI_Grequest_complete(*req);

    if (retval != DEEPER_RETVAL_SUCCESS)
        goto fn_exit_error;

    MPI_Status_set_cancelled(status, 0);
    MPI_Status_set_elements(status, datatype, count);
    status->MPI_SOURCE = MPI_UNDEFINED;
    status->MPI_TAG = MPI_UNDEFINED;

    ADIOI_Free(cb);

    return MPI_SUCCESS;

fn_exit_error:
    ADIOI_Free(cb);

    return MPIO_Err_create_code(MPI_SUCCESS,
                                MPIR_ERR_RECOVERABLE,
                                "ADIOI_BEEGFS_Sync_req_poll",
                                __LINE__, MPI_ERR_IO, "**io %s",
                                strerror(errno));
}

/*
 * ADIOI_BEEGFS_Sync_req_wait -
 */
int ADIOI_BEEGFS_Sync_req_wait(int count, void **array_of_states,
			       double timeout, MPI_Status *status) {
    return MPI_SUCCESS;
}

/*
 * ADIOI_BEEGFS_Sync_req_query -
 */
int ADIOI_BEEGFS_Sync_req_query(void *extra_state, MPI_Status *status) {
    return MPI_SUCCESS;
}

/*
 * ADIOI_BEEGFS_Sync_req_free -
 */
int ADIOI_BEEGFS_Sync_req_free(void *extra_state) {
    return MPI_SUCCESS;
}

/*
 * ADIOI_BEEGFS_Sync_req_cancel -
 */
int ADIOI_BEEGFS_Sync_req_cancel(void *extra_state, int complete) {
    return MPI_SUCCESS;
}

/*
 * vim: ts=8 sts=4 sw=4 noexpandtab
 */
