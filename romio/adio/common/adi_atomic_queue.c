/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 *   Copyright (C) 2014-2016 Seagate Systems UK Ltd.
 *   See COPYRIGHT notice in top-level directory.
 *
 *   Author: Giuseppe Congiu <giuseppe.congiu@seagate.com>
 */
#include "adio.h"
#include "atomic_queue.h"
#include "adi_atomic_queue.h"

/*
 * NOTE: in normal operation the main thread
 *       push(es) elements in the queue and
 *       the sync thread pop(s) them out of
 *       the queue.
 */

/*
 * ADIOI_Sync_req_init - initialise synchronisation request
 *
 * NOTE: pass fields as they are defined in adi_atomic_queue.h
 */
int ADIOI_Sync_req_init(ADIOI_Sync_req_t *r, ...) {

    int type, ret = MPI_SUCCESS;
    va_list args;

    *r = (struct ADIOI_Sync_req *)ADIOI_Malloc(sizeof(struct ADIOI_Sync_req));

    va_start(args, r);
    type = va_arg(args, int);

    switch (type) {
	case ADIOI_THREAD_SYNC:
	    {
		ADIOI_Sync_req_set_key(*r, ADIOI_SYNC_ALL,
			type,
			va_arg(args, ADIO_Offset),
			va_arg(args, MPI_Datatype),
			va_arg(args, int),
			va_arg(args, ADIO_Request*),
			MPI_SUCCESS,
			va_arg(args, int));
	    }
	    break;
	case ADIOI_THREAD_SHUTDOWN:
	    {
		ADIOI_Sync_req_set_key(*r, ADIOI_SYNC_TYPE, type);
		ADIOI_Sync_req_set_key(*r, ADIOI_SYNC_REQ,
			va_arg(args, ADIO_Request*));
		ADIOI_Sync_req_set_key(*r, ADIOI_SYNC_ERR_CODE,
		       	MPI_SUCCESS);
	    }
	    break;
	default:
	    ret = ADIOI_SYNC_REQ_ERR;
    }

    va_end(args);

    return ret;
}

/*
 * ADIOI_Sync_req_init_from - initialise synchronisation request using existing one
 */
int ADIOI_Sync_req_init_from(ADIOI_Sync_req_t *new, ADIOI_Sync_req_t old) {

    return ADIOI_Sync_req_init(new, old->type_, old->off_, old->datatype_,
	    old->count_, old->req_, old->fflags_);
}

/*
 * ADIOI_Sync_req_fini - finilise synchronisation request
 */
int ADIOI_Sync_req_fini(ADIOI_Sync_req_t *r) {

    ADIOI_Free(*r);
    return MPI_SUCCESS;
}

/*
 * ADIOI_Atomic_queue_get_type - return the type of the request
 */
int ADIOI_Sync_req_get_type(ADIOI_Sync_req_t r) {

    return r->type_;
}

/*
 * ADIOI_Atomic_queue_get_key - return the value for key in the request of type "type"
 *
 * INPUT 1 - type
 * INPUT 2 - key
 *
 * INPUT 1 -> ADIOI_THREAD_SYNC
 *   INPUT 2 -> ADIOI_SYNC_OFFSET
 *     OUTPUT 1 -> offset
 *   INPUT 2 -> ADIOI_SYNC_DATATYPE
 *     OUTPUT 1 -> datatype
 *   INPUT 2 -> ADIOI_SYNC_COUNT
 *     OUTPUT 1 -> count
 *   INPUT 2 -> ADIOI_SYNC_REQ
 *     OUTPUT 1 -> req (MPI_Request)
 *   INPUT 2 -> ADIOI_SYNC_ERR_CODE
 *     OUTPUT 1 -> error_code
 *   INPUT 2 -> ADIOI_SYNC_FFLAGS
 *     OUTPUT 1 -> fflags (BeeGFS)
 *   INPUT 2 -> ADIOI_SYNC_ALL
 *     OUTPUT 1 -> offset
 *     OUTPUT 2 -> datatype
 *     OUTPUT 3 -> count
 *     OUTPUT 4 -> req
 *     OUTPUT 5 -> error_code
 *     OUTPUT 6 -> fflags
 * INPUT 1 -> ADIOI_THREAD_SHUTDOWN
 *   INPUT 2 -> ADIOI_SYNC_REQ
 *     OUTPUT 1 -> req
 *   INPUT 2 -> ADIOI_SYNC_ERR_CODE
 *     OUTPUT 1 -> error_code
 */
int ADIOI_Sync_req_get_key(ADIOI_Sync_req_t r, ...) {

    int *error_code, *count, *fflags, ret = MPI_SUCCESS;
    int type, key;
    ADIO_Offset *off;
    ADIO_Request **req;
    MPI_Datatype *datatype;
    va_list args;

    va_start(args, r);
    type = va_arg(args, int);
    key = va_arg(args, int);

    switch (type) {
	case ADIOI_THREAD_SYNC:
	    {
		switch (key) {
		    case ADIOI_SYNC_OFFSET:
			{
			    off = va_arg(args, ADIO_Offset *);
			    *off = r->off_;
			}
			break;
		    case ADIOI_SYNC_DATATYPE:
			{
			    datatype = va_arg(args, MPI_Datatype *);
			    *datatype = r->datatype_;
			}
			break;
		    case ADIOI_SYNC_COUNT:
			{
			    count = va_arg(args, int *);
			    *count = r->count_;
			}
			break;
		    case ADIOI_SYNC_REQ:
			{
			    req = va_arg(args, ADIO_Request **);
			    *req = r->req_;
			}
			break;
		    case ADIOI_SYNC_ERR_CODE:
			{
			    error_code = va_arg(args, int *);
			    *error_code = r->error_code_;
			}
			break;
		    case ADIOI_SYNC_FFLAGS:
			{
			    fflags = va_arg(args, int *);
			    *fflags = r->fflags_;
			}
			break;
		    case ADIOI_SYNC_ALL:
			{
			    /* get memory locations */
			    off = va_arg(args, ADIO_Offset *);
			    datatype = va_arg(args, MPI_Datatype *);
			    count = va_arg(args, int *);
			    req = va_arg(args, ADIO_Request **);
			    error_code = va_arg(args, int *);
			    fflags = va_arg(args, int *);

			    /* set values */
			    *off = r->off_;
			    *datatype = r->datatype_;
			    *count = r->count_;
			    *req = r->req_;
			    *error_code = r->error_code_;
			    *fflags = r->fflags_;
			}
			break;
		    default:
			ret = ADIOI_SYNC_REQ_ERR;
		}
	    }
	    break;
	case ADIOI_THREAD_SHUTDOWN:
	    {
		switch (key) {
		    case ADIOI_SYNC_REQ:
			{
			    req = va_arg(args, ADIO_Request **);
			    *req = r->req_;
			}
			break;
		    case ADIOI_SYNC_ERR_CODE:
			{
			    error_code = va_arg(args, int *);
			    *error_code = r->error_code_;
			}
			break;
		    case ADIOI_SYNC_OFFSET:
		    case ADIOI_SYNC_DATATYPE:
		    case ADIOI_SYNC_COUNT:
		    case ADIOI_SYNC_FFLAGS:
		    case ADIOI_SYNC_ALL:
		    default:
			ret = ADIOI_SYNC_REQ_ERR;
		}
	    }
	    break;
	default:
	    ret = ADIOI_SYNC_REQ_ERR;
    }

    va_end(args);

    return ret;
}

/*
 * ADIOI_Sync_req_set_key - set the value for key
 * INPUT 1 - key
 *
 * INPUT 1 -> ADIOI_SYNC_OFFSET
 *   INPUT 2 -> offset
 * INPUT 1 -> ADIOI_SYNC_DATATYPE
 *   INPUT 2 -> datatype
 * INPUT 1 -> ADIOI_SYNC_COUNT
 *   INPUT 2 -> count
 * INPUT 1 -> ADIOI_SYNC_REQ
 *   INPUT 2 -> req (MPI_Request)
 * INPUT 1 -> ADIOI_SYNC_ERR_CODE
 *   INPUT 2 -> error_code
 * INPUT 1 -> ADIOI_SYNC_FFLAGS
 *   INPUT 2 -> fflags (BeeGFS)
 * INPUT 1 -> ADIOI_SYNC_ALL
 *     INPUT 2 -> offset
 *     INPUT 3 -> datatype
 *     INPUT 4 -> count
 *     INPUT 5 -> req
 *     INPUT 6 -> error_code
 *     INPUT 7 -> fflags
 * INPUT 1 -> ADIOI_SYNC_REQ
 *   INPUT 2 -> req
 * INPUT 1 -> ADIOI_SYNC_ERR_CODE
 *   INPUT 2 -> error_code
 */
int ADIOI_Sync_req_set_key(ADIOI_Sync_req_t r, ...) {

    int key, ret = MPI_SUCCESS;
    va_list args;

    va_start(args, r);
    key = va_arg(args, int);

    switch (key) {
	case ADIOI_SYNC_TYPE:
	    {
		r->type_ = va_arg(args, int);
	    }
	case ADIOI_SYNC_OFFSET:
	    {
	        r->off_ = va_arg(args, ADIO_Offset);
	    }
	    break;
	case ADIOI_SYNC_DATATYPE:
	    {
		r->datatype_ = va_arg(args, MPI_Datatype);
	    }
	    break;
	case ADIOI_SYNC_COUNT:
	    {
		r->count_ = va_arg(args, int);
	    }
	    break;
	case ADIOI_SYNC_REQ:
	    {
		r->req_ = va_arg(args, ADIO_Request *);
	    }
	    break;
	case ADIOI_SYNC_ERR_CODE:
	    {
		r->error_code_ = va_arg(args, int);
	    }
	    break;
	case ADIOI_SYNC_FFLAGS:
	    {
		r->fflags_ = va_arg(args, int);
	    }
	    break;
	case ADIOI_SYNC_ALL:
	    {
		r->type_ = va_arg(args, int);
		r->off_ = va_arg(args, ADIO_Offset);
		r->datatype_ = va_arg(args, MPI_Datatype);
		r->count_ = va_arg(args, int);
		r->req_ = va_arg(args, ADIO_Request *);
		r->error_code_ = va_arg(args, int);
		r->fflags_ = va_arg(args, int);
	    }
	    break;
	default:
		ret = ADIOI_SYNC_REQ_ERR;
    }

    va_end(args);

    return ret;
}

/*
 * ADIOI_Atomic_queue_init - initialise the atomic queue
 */
void ADIOI_Atomic_queue_init(ADIOI_Atomic_queue_t *q) {

    /* alloc mem for queue */
    *q = (struct ADIOI_Atomic_queue *)ADIOI_Malloc(sizeof(struct ADIOI_Atomic_queue));
    INIT_LIST_HEAD(&((*q)->head_));

    /* init mutex and cond variable */
    pthread_mutex_init(&(*q)->lock_, NULL);
    pthread_cond_init(&(*q)->ready_, NULL);

    /* allocate and init empty envelop element */
    struct ADIOI_Sync_req_env *env = (struct ADIOI_Sync_req_env *)
	ADIOI_Malloc(sizeof(struct ADIOI_Sync_req_env));
    env->req_ = NULL;

    /* add empty envelop to the queue */
    list_add_tail(&(env->head_), &((*q)->head_));

    /* init queue size */
    (*q)->size_ = 0;
}

/*
 * ADIOI_Atomic_queue_fini - finalise the atomic queue
 *
 * The callgraph for this function is the following:
 *
 *       ADIOI_xxx_Flush()
 *              |
 *              v
 * ADIOI_Sync_thread_pool_fini()
 *              |
 *              v
 *   ADIOI_Sync_thread_fini()
 *              |
 *              v
 *   ADIOI_Atomic_queue_fini()
 *
 */
void ADIOI_Atomic_queue_fini(ADIOI_Atomic_queue_t *q) {

    struct ADIOI_Sync_req_env *env;

    /* remove and free empty envelop */
    env = list_entry((*q)->head_.prev, struct ADIOI_Sync_req_env, head_);
    ADIOI_Free(env);

    /* fini mutex and cond variable */
    pthread_mutex_destroy(&(*q)->lock_);
    pthread_cond_destroy(&(*q)->ready_);

    /* free atomic queue */
    ADIOI_Free(*q);
}

/*
 * ADIOI_Atomic_queue_empty - returns 1 if the queue is empty
 */
int ADIOI_Atomic_queue_empty(ADIOI_Atomic_queue_t q) {

    return (q->size_ > 0) ? 0 : 1;
}

/*
 * ADIOI_Atomic_queue_size - return the num of elements in the queue
 */
int ADIOI_Atomic_queue_size(ADIOI_Atomic_queue_t q) {

    return q->size_;
}

/*
 * ADIOI_Atomic_queue_front - return the oldest element in the queue
 */
ADIOI_Sync_req_t ADIOI_Atomic_queue_front(ADIOI_Atomic_queue_t q) {

    struct ADIOI_Sync_req_env *env;

    pthread_mutex_lock(&q->lock_);
    while (ADIOI_Atomic_queue_empty(q))
	pthread_cond_wait(&q->ready_, &q->lock_);

    env = list_entry(q->head_.next, struct ADIOI_Sync_req_env, head_);

    pthread_mutex_unlock(&q->lock_);

    return env->req_;
}

/*
 * ADIOI_Atomic_queue_back - return the youngest element in the queue
 */
ADIOI_Sync_req_t ADIOI_Atomic_queue_back(ADIOI_Atomic_queue_t q) {

    struct ADIOI_Sync_req_env *env;

    pthread_mutex_lock(&q->lock_);
    while (ADIOI_Atomic_queue_empty(q))
	pthread_cond_wait(&q->ready_, &q->lock_);

    env = list_entry(q->head_.prev->prev, struct ADIOI_Sync_req_env, head_);

    pthread_mutex_unlock(&q->lock_);

    return env->req_;
}

/*
 * ADIOI_Atomic_queue_push - push an element in the queue
 *
 * The element memory has been already allocated in the main thread.
 */
void ADIOI_Atomic_queue_push(ADIOI_Atomic_queue_t q, ADIOI_Sync_req_t r) {

    struct ADIOI_Sync_req_env *env;
    int size;

    pthread_mutex_lock(&q->lock_);

    size = q->size_;

    /* get empty envelop */
    env = list_entry(q->head_.prev, struct ADIOI_Sync_req_env, head_);

    /* update request field in envelop */
    env->req_ = r;

    /* create a new empty envelop */
    env = (struct ADIOI_Sync_req_env *)
	ADIOI_Malloc(sizeof(struct ADIOI_Sync_req_env));
    env->req_ = NULL;

    /* add new empty envelop to queue */
    list_add_tail(&(env->head_), &(q->head_));

    /* increment size of the queue */
    q->size_++;

    pthread_mutex_unlock(&q->lock_);

    if (size == 0)
	pthread_cond_signal(&q->ready_);
}

/*
 * ADIOI_Atomic_queue_pop - removes the tail element from the queue
 */
void ADIOI_Atomic_queue_pop(ADIOI_Atomic_queue_t q) {

    struct ADIOI_Sync_req_env *env;

    pthread_mutex_lock(&q->lock_);
    while (ADIOI_Atomic_queue_empty(q))
	pthread_cond_wait(&q->ready_, &q->lock_);

    /* remove front envelop */
    env = list_entry(q->head_.next, struct ADIOI_Sync_req_env, head_);
    list_del(&(env->head_));
    ADIOI_Free(env);

    /* decrement size of queue */
    q->size_--;

    pthread_mutex_unlock(&q->lock_);
}

/*
 * vim: ts=8 sts=4 sw=4 noexpandtab
 */
