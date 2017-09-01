/*
  Copyright (c) 2017 Red Hat, Inc. <http://www.redhat.com>
  This file is part of GlusterFS.

  This file is licensed to you under your choice of the GNU Lesser
  General Public License, version 3 or any later version (LGPLv3 or
  later), or the GNU General Public License, version 2 (GPLv2), in all
  cases as published by the Free Software Foundation.
*/

/* File: rio-server-fops.c
 * This file contains the RIO MDC/server FOP entry and exit points.
 */

#include "xlator.h"

#include "rio-common.h"
#include "defaults.h"
#include "rio-server-fops.h"
#include "layout.h"

/* LOOKUP Callback
If lookup returns an inode, the resolution is complete and we need the
checks as follows,
        - dirty inode handling
        - xaction handling
        - layout unlocking
If lookup returns EREMOTE, then only layout unlocking has to happen, as other
actions are done when we have the actual inode being processed.
*/
int32_t rio_server_lookup_cbk (call_frame_t *frame, void *cookie,
                               xlator_t *this, int32_t op_ret, int32_t op_errno,
                               inode_t *inode, struct iatt *buf, dict_t *xdata,
                               struct iatt *postparent)
{
        VALIDATE_OR_GOTO (frame, bailout);

        /* TODO: dirty inode handling
        If inode is dirty, then we need to fetch further stat from DS,
        or trust upcall/mdcache? */

        /* TODO: xaction handling
        If inode is part of an ongoing xaction, we need saved/stored
        iatt information that we will respond with
                - inode->xactioninprogress
                - inode->savedlinkcount */

        /* TODO: layout unlocks */

        /* NOTE: Yes, if we are just unwinding, use STACK_WIND_TAIL, but we
        are sure to expand this in the future to address the above TODOs hence
        using this WIND/UNWIND scheme. NOTE will be removed as function
        expands. */
        STACK_UNWIND_STRICT (lookup, frame, op_ret, op_errno, inode, buf,
                             xdata, postparent);
bailout:
        return 0;
}

/* LOOKUP Call
Lookup on MDS is passed directly to POSIX, which will return all information
or EREMOTE if inode is not local. Client handles EREMOTE resolution with the
correct MDS further.

Generic structure:
        - lock the layout
        - check if layout version is in agreement between client and MDS
        - mark the layout as in use (to prevent changes to the layout when
        FOPs are being processed)
        - WIND to the next xlator
*/
int32_t rio_server_lookup (call_frame_t *frame, xlator_t *this, loc_t *loc,
                           dict_t *xdata)
{
        xlator_t *subvol;
        struct rio_conf *conf;

        VALIDATE_OR_GOTO (frame, bailout);
        VALIDATE_OR_GOTO (this, error);
        conf = this->private;
        VALIDATE_OR_GOTO (conf, error);

        /* TODO: layout checks and locks */

        subvol = conf->riocnf_server_local_xlator;

        STACK_WIND (frame, rio_server_lookup_cbk, subvol, subvol->fops->lookup,
                    loc, xdata);
        return 0;

error:
        STACK_UNWIND (lookup, frame, -1, errno, NULL, NULL, NULL, NULL);
        return 0;
bailout:
        return 0;
}

int32_t
rio_server_create_cbk (call_frame_t *frame, void *cookie, xlator_t *this,
                       int32_t op_ret, int32_t op_errno, fd_t *fd,
                       inode_t *inode, struct iatt *buf, struct iatt *preparent,
                       struct iatt *postparent, dict_t *xdata)
{
        VALIDATE_OR_GOTO (frame, bailout);

        /* on success, or unhandled failures, unwind */
        if (!op_ret || (op_ret != 0 && op_errno != EREMOTE)) {
                STACK_UNWIND (create, frame, op_ret, op_errno, fd, inode,
                              buf, preparent, postparent, xdata);
                return 0;
        }

        /* TODO: We need to handle the EREMOTE case, so for now the
         * unhandled part is just maintained as a special exception */
        /* (op_errno == EREMOTE)) */
        STACK_UNWIND (create, frame, op_ret, op_errno, fd, inode,
                      buf, preparent, postparent, xdata);
bailout:
        return 0;
}

/* CREATE Call
Create is passed to POSIX, and RIO does not fork the icreate and namelink
operations, as 2 FOP calls. POSIX may return EREMOTE, if the file already
exists, in which case create needs to further open the remote inode.

Generic structure:
        - generate a GFID that will colocate the file inode with the directory
        inode (same bucket #)
        - lock the layout
        - check if layout version is in agreement between client and MDS
        - mark the layout as in use (to prevent changes to the layout when
        FOPs are being processed)
        - WIND to the next xlator
*/
int32_t
rio_server_create (call_frame_t *frame, xlator_t *this, loc_t *loc,
                   int32_t flags, mode_t mode, mode_t umask, fd_t *fd,
                   dict_t *xdata)
{
        xlator_t *subvol;
        struct rio_conf *conf;
        uuid_t gfid_req;
        int ret;

        VALIDATE_OR_GOTO (frame, bailout);
        VALIDATE_OR_GOTO (this, error);
        conf = this->private;
        VALIDATE_OR_GOTO (conf, error);

        /* fill in gfid-req for entry creation, to colocate file inode with the
         * parent */
        /* TODO: Trust loc->pargfid in all cases? */
        layout_generate_colocated_gfid (conf->riocnf_mdclayout, gfid_req,
                                        loc->parent->gfid);

        /* TODO: gfid-req is something other parts of the code use as well. Need
         * to understand how this would work, when we overwrite the same with
         * our requested GFID */
        ret = dict_set_static_bin (xdata, "gfid-req", gfid_req, 16);
        if (ret) {
                errno = ENOMEM;
                goto error;
        }

        /* TODO: layout checks and locks */
        subvol = conf->riocnf_server_local_xlator;

        /* wind call to subvolume */
        STACK_WIND (frame, rio_server_create_cbk,
                    subvol, subvol->fops->create,
                    loc, flags, mode, umask, fd, xdata);

        return 0;
error:
        STACK_UNWIND (create, frame, -1, errno, NULL, NULL, NULL,
                      NULL, NULL, NULL);
bailout:
        return 0;
}

int32_t
rio_server_mkdir (call_frame_t *frame, xlator_t *this,
                  loc_t *loc, mode_t mode, mode_t umask, dict_t *xdata)
{
        return 0;
}
