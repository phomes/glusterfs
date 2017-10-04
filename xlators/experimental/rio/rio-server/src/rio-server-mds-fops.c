/*
  Copyright (c) 2017 Red Hat, Inc. <http://www.redhat.com>
  This file is part of GlusterFS.

  This file is licensed to you under your choice of the GNU Lesser
  General Public License, version 3 or any later version (LGPLv3 or
  later), or the GNU General Public License, version 2 (GPLv2), in all
  cases as published by the Free Software Foundation.
*/

/* File: rio-server-mds-fops.c
 * This file contains the RIO MDC/server FOP entry and exit points.
 */

#include "xlator.h"

#include "rio-common.h"
#include "rio-common-helpers.h"
#include "rio-server-mds-fops.h"
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

int32_t rio_server_mkdir_namelink_cbk (call_frame_t *frame, void *cookie,
                                       xlator_t *this, int32_t op_ret,
                                       int32_t op_errno, struct iatt *prebuf,
                                       struct iatt *postbuf, dict_t *xdata)
{
        struct rio_local *local;

        VALIDATE_OR_GOTO (frame, bailout);
        local = frame->local;

        /* TODO: Orphan inode and journal */
        /* TODO: xdata should be merged from icreate and namelink and sent
        back? */
        STACK_UNWIND (mkdir, frame, op_ret, op_errno, local->riolocal_inode,
                      &local->riolocal_stbuf, prebuf, postbuf, xdata);
        rio_local_wipe (local);
        return 0;
bailout:
        return -1;
}

int32_t rio_server_mkdir_namelink (call_frame_t *frame, xlator_t *this,
                                   struct rio_local *local)
{
        xlator_t *subvol;
        struct rio_conf *conf;

        conf = this->private;
        VALIDATE_OR_GOTO (conf, error);

        /* TODO: layout checks and locks */
        subvol = conf->riocnf_server_local_xlator;

        /* TODO: namelink does not bother with umask? We need to for ACL
        checking to be right IMO. Further, umask should have been processed by
        the ACL, so that mode is adjusted appropriately. Needs further
        thought */
        STACK_WIND (frame, rio_server_mkdir_namelink_cbk, subvol,
                    subvol->fops->namelink, &local->riolocal_loc,
                    local->riolocal_xdata_in);
        return 0;
 error:
        rio_local_wipe (local);
        STACK_UNWIND (mkdir, frame, -1, errno, NULL, NULL, NULL, NULL, NULL);
        return -1;
}

/* ICREATE callback
icreate can be called only for mkdir (as of this writing). Hence this assumes
that this is a directory inode creation callback and acts accordingly.

Generic structure:
        - If creation failed, fail the FOP
        - Else, wind the namelink to the local MDS if we are the caller of
        mkdir
        - If we are not, then just unwind
*/
int32_t
rio_server_icreate_cbk (call_frame_t *frame, void *cookie, xlator_t *this,
                        int32_t op_ret, int32_t op_errno, inode_t *inode,
                        struct iatt *buf, dict_t *xdata)
{
        struct rio_local *local;
        glusterfs_fop_t infop;
        int32_t ret = 0;

        VALIDATE_OR_GOTO (frame, bailout);
        local = frame->local;
        /* Without infop cannot unwind correctly, hence bailout */
        VALIDATE_OR_GOTO (local, bailout);
        infop = local->riolocal_fop;
        VALIDATE_OR_GOTO (this, error);

        /* on unhandled failures, unwind */
        if (op_ret) {
                rio_local_wipe (local);
                if (infop == GF_FOP_MKDIR)
                        STACK_UNWIND (mkdir, frame, op_ret, op_errno, inode,
                                      buf, NULL, NULL, xdata);
                /* TODO: When icreate is added, add an unwind
                condition for the same */
                return 0;
        }

        /* Stash the inode and stat information to return when namelink
        completes */
        rio_iatt_copy (&local->riolocal_stbuf, buf);
        local->riolocal_inode = inode_ref (inode);
        if (xdata)
                local->riolocal_xdata_out = dict_ref (xdata);

        if (infop == GF_FOP_MKDIR) {
                ret = rio_server_mkdir_namelink (frame, this, local);
        }
        /* TODO: When icreate is added, add an unwind condition for the same */
        return ret;
error:
        rio_local_wipe (local);
        if (infop == GF_FOP_MKDIR) {
                STACK_UNWIND (mkdir, frame, -1, errno, NULL, NULL, NULL, NULL,
                              NULL);
        }
bailout:
        return -1;
}

/* MKDIR call
Mkdir is a transaction, where it would create the inode for the directory, and
then link in the name for the directory under the parent. The inode can reside
in a totally different MDS than the name. There is hence a need to maintain a
journal for reclaiming orphan inodes among other coordination to be done in this
call.

Generic structure:
        - lock the layout
        - check if layout version is in agreement between client and MDS
        - mark the layout as in use (to prevent changes to the layout when
        FOPs are being processed)
        - Journal the OP
        - Prepare to WIND the Op to create the inode
        - WIND inode creation to the right MDS
        - On the return, WIND namelink to the local MDS
*/
int32_t
rio_server_mkdir (call_frame_t *frame, xlator_t *this,
                  loc_t *loc, mode_t mode, mode_t umask, dict_t *xdata)
{
        xlator_t *subvol;
        struct rio_conf *conf;
        struct rio_local *local;
        void *uuidreq;
        uuid_t gfid;
        loc_t inode_loc = {0,};
        int ret;

        VALIDATE_OR_GOTO (frame, bailout);
        VALIDATE_OR_GOTO (this, error);
        conf = this->private;
        VALIDATE_OR_GOTO (conf, error);
        VALIDATE_OR_GOTO (loc, error);
        VALIDATE_OR_GOTO (loc->name, error);

        /* TODO: layout checks and locks */
        /* TODO: journal work */

        /* Determine MDS to create inode in */
        /* NOTE: We use the already generated GFID, as that ensures enough
        randomness to give us a bucket ID distribution */
        ret = dict_get_ptr (xdata, "gfid-req", &uuidreq);
        if (ret) {
                gf_msg (this->name, GF_LOG_ERROR, errno,
                        RIO_MSG_LAYOUT_ERROR,
                        "Missing GFID in request data for creating directory"
                        " %s, under parent %s", loc->name,
                        uuid_utoa (loc->parent->gfid));
                errno = EINVAL;
                goto error;
        }
        gf_uuid_copy (gfid, uuidreq);

        /* Find MDS to redirect inode request to */
        subvol = layout_search (conf->riocnf_mdclayout, gfid);
        if (!subvol) {
                gf_msg (this->name, GF_LOG_ERROR, errno,
                        RIO_MSG_LAYOUT_ERROR,
                        "Unable to find subvolume for GFID %s,"
                        "failing directory creation of %s, under parent %s",
                        uuid_utoa (gfid), loc->name,
                        uuid_utoa (loc->parent->gfid));
                errno = EINVAL;
                goto error;
        }

        /* Generate a loc for icreate operation */
        rio_prepare_inode_loc (&inode_loc, loc->inode, gfid, _gf_true);

        /* Stash the request */
        local = rio_local_init (frame, conf, loc, NULL, xdata, GF_FOP_MKDIR);
        if (!local) {
                gf_msg (this->name, GF_LOG_ERROR, errno,
                        RIO_MSG_LOCAL_ERROR,
                        "Unable to store FOP in args for operation continuty,"
                        "failing directory creation of %s, under parent %s",
                        loc->name, uuid_utoa (loc->parent->gfid));
                errno = ENOMEM;
                goto error;
        }

        /* Wind the icreate to create the inode for the directory */
        STACK_WIND (frame, rio_server_icreate_cbk, subvol,
                    subvol->fops->icreate, &inode_loc, S_IFDIR | mode, xdata);
        loc_wipe (&inode_loc);
        return 0;
error:
        rio_local_wipe (local);
        loc_wipe (&inode_loc);
        STACK_UNWIND (mkdir, frame, -1, errno, NULL, NULL, NULL, NULL, NULL);
bailout:
        return -1;
}
