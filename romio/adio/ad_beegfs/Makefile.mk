## -*- Mode: Makefile; -*-
## vim: set ft=automake :
##
## (C) 2011 by Argonne National Laboratory.
##     See COPYRIGHT in top-level directory.
##

if BUILD_AD_BEEGFS

noinst_HEADERS += adio/ad_beegfs/ad_beegfs.h

romio_other_sources +=                     \
    adio/ad_beegfs/ad_beegfs.c             \
    adio/ad_beegfs/ad_beegfs_hints.c	   \
    adio/ad_beegfs/ad_beegfs_open.c        \
    adio/ad_beegfs/ad_beegfs_close.c 	   \
    adio/ad_beegfs/ad_beegfs_cache_sync.c  \
    adio/ad_beegfs/ad_beegfs_opencoll.c    \
    adio/ad_beegfs/ad_beegfs_write.c       \
    adio/ad_beegfs/ad_beegfs_flush.c

endif BUILD_AD_BEEGFS

