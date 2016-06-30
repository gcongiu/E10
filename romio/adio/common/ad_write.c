/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/* 
 *
 *   Copyright (C) 2004 University of Chicago. 
 *   See COPYRIGHT notice in top-level directory.
 *
 *   Copyright (C) 2014-2016 Seagate Systems UK Ltd.
 */

#include "adio.h"

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef AGGREGATION_PROFILE
#include "mpe.h"
#endif

void ADIOI_GEN_WriteContig(ADIO_File fd, const void *buf, int count,
			   MPI_Datatype datatype, int file_ptr_type,
			   ADIO_Offset offset, ADIO_Status *status,
			   int *error_code)
{
    off_t err_lseek = -1;
    ssize_t err = -1;
    MPI_Count datatype_size;
    ADIO_Offset len, bytes_xfered=0;
    ADIO_File fh = fd;
    size_t wr_count;
    static char myname[] = "ADIOI_GEN_WRITECONTIG";
    char * p;

#ifdef AGGREGATION_PROFILE
    MPE_Log_event (5036, 0, NULL);
#endif

    MPI_Type_size_x(datatype, &datatype_size);
    len = (ADIO_Offset)datatype_size * (ADIO_Offset)count;

    /* if using the cache select the cache file handle */
    if (fd->cache_fd && fd->cache_fd->is_open) {
	fh = fd->cache_fd;

	/* if cache is coherent lock global file extent */ 
	if (fd->hints->e10_cache_coherent == ADIOI_HINT_ENABLE)
	    ADIOI_WRITE_LOCK(fd, offset, SEEK_SET, len);
    }

    if (file_ptr_type == ADIO_INDIVIDUAL) {
	offset = fh->fp_ind;
    }

    if (fh->fp_sys_posn != offset) {
#ifdef ADIOI_MPE_LOGGING
        MPE_Log_event( ADIOI_MPE_lseek_a, 0, NULL );
#endif
	err_lseek = lseek(fh->fd_sys, offset, SEEK_SET);
#ifdef ADIOI_MPE_LOGGING
        MPE_Log_event( ADIOI_MPE_lseek_b, 0, NULL );
#endif
	/* --BEGIN ERROR HANDLING-- */
	if (err_lseek == -1) {
	    *error_code = MPIO_Err_create_code(MPI_SUCCESS,
					       MPIR_ERR_RECOVERABLE,
					       myname, __LINE__,
					       MPI_ERR_IO, "**io",
					       "**io %s", strerror(errno));
	    fh->fp_sys_posn = -1;
	    return;
	}
	/* --END ERROR HANDLING-- */
    }
    
    p = (char *)buf;
    while (bytes_xfered < len) {
#ifdef ADIOI_MPE_LOGGING
	MPE_Log_event( ADIOI_MPE_write_a, 0, NULL );
#endif
	wr_count = len - bytes_xfered;
	err = write(fh->fd_sys, p, wr_count);
	/* --BEGIN ERROR HANDLING-- */
	if (err == -1) {
	    *error_code = MPIO_Err_create_code(MPI_SUCCESS,
		    MPIR_ERR_RECOVERABLE,
		    myname, __LINE__,
		    MPI_ERR_IO, "**io",
		    "**io %s", strerror(errno));
	    fh->fp_sys_posn = -1;
	    return;
	}
    /* --END ERROR HANDLING-- */
#ifdef ADIOI_MPE_LOGGING
	MPE_Log_event( ADIOI_MPE_write_b, 0, NULL );
#endif
	bytes_xfered += err;
	p += err;
    }

    fh->fp_sys_posn = offset + bytes_xfered;

    if (file_ptr_type == ADIO_INDIVIDUAL) {
	fh->fp_ind += bytes_xfered; 
    }

    /* if flush immediate flush cache here */
    if (fd->cache_fd && fd->cache_fd->is_open && 
	    fd->hints->e10_cache_flush_flag != ADIOI_HINT_FLUSHNONE) {

	ADIOI_Sync_req_t sub;
	int threads, curr_thread, idx;
	ADIO_Request *r = (ADIO_Request *)ADIOI_Malloc(sizeof(ADIO_Request));

	*r = MPI_REQUEST_NULL;
	threads = fd->hints->e10_cache_threads;
	curr_thread = fd->thread_curr;
	idx = curr_thread % threads;

	/* init sync req */
	ADIOI_Sync_req_init(&sub, ADIOI_THREAD_SYNC, offset, datatype, count, r, 0);

	/* enqueue sync request to thread and flush */
	ADIOI_Sync_thread_enqueue(fd->thread_pool[idx], sub);
	
	if (fd->hints->e10_cache_flush_flag == ADIOI_HINT_FLUSHIMMEDIATE)
	    ADIOI_Sync_thread_flush(fd->thread_pool[idx]);
	
	/* select next thread in the pool */
	fd->thread_curr = (curr_thread + 1) % threads;
    }

#ifdef HAVE_STATUS_SET_BYTES
    /* bytes_xfered could be larger than int */
    if (err != -1 && status) MPIR_Status_set_bytes(status, datatype, bytes_xfered);
#endif

    *error_code = MPI_SUCCESS;
#ifdef AGGREGATION_PROFILE
    MPE_Log_event (5037, 0, NULL);
#endif
}

/* 
 * vim: ts=8 sts=4 sw=4 noexpandtab 
 */
