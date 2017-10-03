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
        int32_t      ret      = 0;
        int          entrylen = 0;
        char        *entry    = NULL;
        uuid_t       tgtuuid  = {0,};
        struct iatt  buf      = {0,};
        struct iatt  pbuf     = {0,};
        struct posix_private *priv = NULL;

        priv = this->private;

        entrylen = posix2_handle_length (priv->base_path_length);
        entry = alloca (entrylen);

        if (loc->inode && !gf_uuid_is_null (loc->inode->gfid))
                gf_uuid_copy (tgtuuid, loc->inode->gfid);
        else
                gf_uuid_copy (tgtuuid, loc->gfid);

        errno = EINVAL;
        ret = posix2_make_handle (tgtuuid, priv->base_path, entry, entrylen);
        if (ret != 0)
                goto unwind_err;

        ret = posix2_istat_path (this, tgtuuid, entry, &buf, _gf_false);
        if (ret == 0) {
                goto done;
        } else {
                if (errno != ENOENT)
                        goto unwind_err;
        }

        /* On the DS the data inode/block is created only when there are any
        data operations. This means that for a lookup of this object (which is
        initiated when there is an fd based data operation) is when it is
        create first. Hence failed lookups with ENOENT error # is created and
        the results sent back to the resolver. */
        /* TODO: the data inode is created wit 0600 permissions, IOW a non-root
        client attempting to write to the file may not suceed? */
        ret = posix2_create_inode (this, entry, 0, 0600);
        if (ret) {
                goto unwind_err;
        } else {
                ret = posix2_istat_path (this, tgtuuid, entry, &buf, _gf_false);
                if (ret)
                        goto unwind_err;
        }
done:
        STACK_UNWIND_STRICT (lookup, frame, 0, 0, loc->inode, &buf, NULL,
                             &pbuf);
        return 0;

 unwind_err:
        STACK_UNWIND_STRICT (lookup, frame, -1,
                             EINVAL, NULL, NULL, NULL, NULL);
        return 0;
}
