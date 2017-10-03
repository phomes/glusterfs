/*
  Copyright (c) 2017 Red Hat, Inc. <http://www.redhat.com>
  This file is part of GlusterFS.

  This file is licensed to you under your choice of the GNU Lesser
  General Public License, version 3 or any later version (LGPLv3 or
  later), or the GNU General Public License, version 2 (GPLv2), in all
  cases as published by the Free Software Foundation.
*/

/* File: posix2-ds-fops.c
 * This file contains posix2 FOPs that are specific to DS bricks.
 */

#include "xlator.h"
#include "syscall.h"
#include "common-utils.h"
#include "posix.h"

#include "posix2-helpers.h"

int32_t
posix2_ds_lookup (call_frame_t *frame,
                  xlator_t *this, loc_t *loc, dict_t *xdata)
{
        STACK_UNWIND_STRICT (lookup, frame, -1,
                             ENOTSUP, NULL, NULL, NULL, NULL);
        return 0;
}
