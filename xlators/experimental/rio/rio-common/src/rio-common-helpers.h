/*
  Copyright (c) 2016 Red Hat, Inc. <http://www.redhat.com>
  This file is part of GlusterFS.

  This file is licensed to you under your choice of the GNU Lesser
  General Public License, version 3 or any later version (LGPLv3 or
  later), or the GNU General Public License, version 2 (GPLv2), in all
  cases as published by the Free Software Foundation.
*/

/* File: rio-common-helpers.h
 * This file contains common rio helper routines
 */

#ifndef _RIO_COMMON_HELPERS_H
#define _RIO_COMMON_HELPERS_H

#include "list.h"
#include "layout.h"
#include "rio-common.h"

int rio_process_volume_lists (xlator_t *, struct rio_conf *);
void rio_destroy_volume_lists (struct rio_conf *);
int32_t rio_lookup_is_nameless (loc_t *);
struct rio_local *rio_local_init (call_frame_t *, struct rio_conf *, loc_t *,
                                  fd_t *, dict_t *, glusterfs_fop_t);
void rio_local_wipe (struct rio_local *);
void rio_prepare_inode_loc (loc_t *dst, inode_t *src, uuid_t,
                            gf_boolean_t auxparent);
void rio_iatt_merge_ds_mds (struct iatt *, struct iatt *);
void rio_iatt_merge_mds_ds (struct iatt *, struct iatt *);
void rio_iatt_cleanse_ds (struct iatt *);
void rio_iatt_copy (struct iatt *, struct iatt *);
gf_boolean_t rio_is_inode_dirty (inode_t *);

#endif /* _RIO_COMMON_HELPERS_H */
