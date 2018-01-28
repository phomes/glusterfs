/*
  Copyright (c) 2016 Red Hat, Inc. <http://www.redhat.com>
  This file is part of GlusterFS.

  This file is licensed to you under your choice of the GNU Lesser
  General Public License, version 3 or any later version (LGPLv3 or
  later), or the GNU General Public License, version 2 (GPLv2), in all
  cases as published by the Free Software Foundation.
*/

/* File: rio-client-main.c
 * This file contains the xlator loading functions, FOP entry points
 * and options.
 * The entire functionality including comments is TODO.
 */

#include "compat.h"
#include "compat-errno.h"
#include "xlator.h"
#include "locking.h"
#include "defaults.h"

#include "rio-common.h"
#include "rio-common-helpers.h"
#include "rio-client-autogen-fops.h"
#include "layout.h"

int32_t rio_client_lookup_remote_cbk (call_frame_t *frame, void *cookie,
                                      xlator_t *this, int32_t op_ret,
                                      int32_t op_errno, inode_t *inode,
                                      struct iatt *buf, dict_t *xdata,
                                      struct iatt *postparent)
{
        struct rio_local *local;

        VALIDATE_OR_GOTO (frame, bailout);
        local = frame->local;
        VALIDATE_OR_GOTO (local, error);

        frame->local = NULL;
        STACK_UNWIND_STRICT (lookup, frame, op_ret, op_errno, inode, buf,
                             xdata, &local->riolocal_stbuf);
        rio_local_wipe (local);
        return 0;
error:
        STACK_UNWIND (lookup, frame, -1, errno, NULL, NULL, NULL, NULL);
bailout:
        return 0;
}

int32_t rio_client_lookup_remote (call_frame_t *frame, xlator_t *this,
                                  struct rio_local *local, uuid_t tgtgfid)
{
        xlator_t *subvol;
        struct rio_conf *conf;
        loc_t inode_loc = {0,};

        VALIDATE_OR_GOTO (frame, bailout);
        conf = this->private;
        VALIDATE_OR_GOTO (conf, error);

        /* Generate a loc for gfid lookup operation */
        rio_prepare_inode_loc (&inode_loc, local->riolocal_inode, tgtgfid,
                               _gf_false);

        subvol = layout_search (conf->riocnf_mdclayout, tgtgfid);
        if (!subvol) {
                gf_msg (this->name, GF_LOG_ERROR, errno,
                        RIO_MSG_LAYOUT_ERROR,
                        "Unable to find subvolume for GFID %s",
                        uuid_utoa (tgtgfid));
                errno = EINVAL;
                goto error;
        }

        STACK_WIND (frame, rio_client_lookup_remote_cbk, subvol,
                    subvol->fops->lookup, &inode_loc, NULL);
        return 0;
 error:
        frame->local = NULL;
        rio_local_wipe (local);
        STACK_UNWIND (lookup, frame, -1, errno, NULL, NULL, NULL, NULL);
bailout:
        return 0;
}

/* LOOKUP Callback
Handle the EREMOTE error, in case the name and inode for the name are on
different MDS subvolumes.

Other than the above special case, no other handling is required in LOOKUP.
*/
int32_t rio_client_lookup_cbk (call_frame_t *frame, void *cookie,
                               xlator_t *this, int32_t op_ret, int32_t op_errno,
                               inode_t *inode, struct iatt *buf, dict_t *xdata,
                               struct iatt *postparent)
{
        struct rio_local *local;
        int32_t ret;

        VALIDATE_OR_GOTO (frame, bailout);
        VALIDATE_OR_GOTO (this, error);
        local = frame->local;
        VALIDATE_OR_GOTO (local, error);

        /* Chase the gfid on EREMOTE errors */
        if (op_ret == -1 && op_errno == EREMOTE) {
                if (!buf || gf_uuid_is_null (buf->ia_gfid)) {
                        /* EREMOTE is never for nameless lookup */
                        gf_msg (this->name, GF_LOG_ERROR, errno,
                                RIO_MSG_MISSING_GFID,
                                "Missing GFID for remote inode,"
                                " failing lookup of name %s under"
                                " parent gfid %s", local->riolocal_loc.name,
                                uuid_utoa (local->riolocal_loc.pargfid));
                        errno = ENODATA;
                        goto error;
                }
                local->riolocal_inode = inode_ref (inode);
                rio_iatt_copy (&local->riolocal_stbuf, buf);
                if (xdata)
                        local->riolocal_xdata_out = dict_ref (xdata);

                ret = rio_client_lookup_remote (frame, this, local,
                                                buf->ia_gfid);
                return ret;
        }

        frame->local = NULL;
        rio_local_wipe (local);
        STACK_UNWIND_STRICT (lookup, frame, op_ret, op_errno, inode, buf,
                             xdata, postparent);
        return 0;
error:
        frame->local = NULL;
        rio_local_wipe (local);
        STACK_UNWIND (lookup, frame, -1, errno, NULL, NULL, NULL, NULL);
bailout:
        return 0;
}

/* LOOKUP Call
Lookup on MDS is passed directly to POSIX, which will return all information
or EREMOTE if inode is not local. Client handles EREMOTE resolution with the
correct MDS further, on the unwind path.
*/
int32_t rio_client_lookup (call_frame_t *frame, xlator_t *this, loc_t *loc,
                           dict_t *xdata)
{
        xlator_t *subvol;
        struct rio_conf *conf;
        struct rio_local *local;
        uuid_t searchuuid;

        VALIDATE_OR_GOTO (frame, bailout);
        VALIDATE_OR_GOTO (this, error);
        conf = this->private;
        VALIDATE_OR_GOTO (conf, error);
        VALIDATE_OR_GOTO (loc, error);

        /* Nameless lookup: Even if it is a revalidate, does not matter, just
        lookup the inode, there is no other information to go by */
        /* Named lookup:
          - Case 1: fresh lookup, so send it to pGFID MDS
          - Case 2: Revalidate, which essentially means, check if name and
          and inode are valid, so send to pGFID
        TODO: Tests and expectations when lookup comes wit another client
        having performed a rename or a recreate */
        /* Decide where to redirect the lookup, to the parent or to the gfid */
        if (loc_is_nameless (loc)) {
                if ((loc->inode != NULL) &&
                    !gf_uuid_is_null (loc->inode->gfid)) {
                        gf_uuid_copy(searchuuid, loc->inode->gfid);
                } else if (!gf_uuid_is_null (loc->gfid)) {
                        gf_uuid_copy(searchuuid, loc->gfid);
                } else {
                        gf_msg (this->name, GF_LOG_ERROR, errno,
                                RIO_MSG_MISSING_GFID,
                                "Unable to find GFID for nameless lookup");
                        errno = EINVAL;
                        goto error;
                }
        } else {
                if (loc->parent && !gf_uuid_is_null (loc->parent->gfid)) {
                        gf_uuid_copy (searchuuid, loc->parent->gfid);
                } else if (!gf_uuid_is_null (loc->pargfid)) {
                        gf_uuid_copy (searchuuid, loc->pargfid);
                } else {
                        gf_msg (this->name, GF_LOG_ERROR, errno,
                                RIO_MSG_MISSING_GFID,
                                "Unable to find parent GFID for named lookup");
                        errno = EINVAL;
                        goto error;
                }
        }

        subvol = layout_search (conf->riocnf_mdclayout, searchuuid);
        if (!subvol) {
                gf_msg (this->name, GF_LOG_ERROR, errno,
                        RIO_MSG_LAYOUT_ERROR,
                        "Unable to find subvolume for GFID %s",
                        uuid_utoa (searchuuid));
                errno = EINVAL;
                goto error;
        }

        /* Stash the request */
        local = rio_local_init (frame, conf, loc, NULL, xdata, GF_FOP_LOOKUP);
        if (!local) {
                if (loc_is_nameless (loc)) {
                        gf_msg (this->name, GF_LOG_ERROR, errno,
                                RIO_MSG_LOCAL_ERROR,
                                "Unable to store FOP in args for operation"
                                " continuty, failing lookup of gfid %s",
                                uuid_utoa (searchuuid));
                } else {
                        gf_msg (this->name, GF_LOG_ERROR, errno,
                                RIO_MSG_LOCAL_ERROR,
                                "Unable to store FOP in args for operation"
                                " continuty, failing lookup of name %s under"
                                " parent gfid %s", loc->name,
                                uuid_utoa (searchuuid));
                }
                errno = ENOMEM;
                goto error;
        }

#ifdef RIO_DEBUG
        gf_msg (this->name, GF_LOG_INFO, 0, 0,
                "Sending LOOKUP for name - %s, pargfid - %s, gfid - %s"
                " to subvol %s", loc->name, uuid_utoa (loc->pargfid),
                uuid_utoa (loc->gfid), subvol->name);
#endif
        STACK_WIND (frame, rio_client_lookup_cbk, subvol, subvol->fops->lookup,
                    loc, xdata);
        return 0;
error:
        frame->local = NULL;
        rio_local_wipe (local);
        STACK_UNWIND (lookup, frame, -1, errno, NULL, NULL, NULL, NULL);
bailout:
        return 0;
}

int
rio_client_notify (xlator_t *this, int32_t event, void *data, ...)
{
        int ret = 0;

        /* TODO: Pretty darn unsure about this!
        Need to check this better and see if the following actions are right */
        switch (event) {
        case GF_EVENT_CHILD_UP:
        {
                struct rio_conf *conf = this->private;
                int count;

                /* HACK: keep a count of child up events and return when all
                are connected */
                LOCK (&(conf->riocnf_lock));
                count = ++conf->riocnf_notify_count;
                UNLOCK (&(conf->riocnf_lock));

                if (count == (conf->riocnf_dc_count + conf->riocnf_mdc_count)) {
                        ret = default_notify (this, event, data);
                }
        }
        break;
        default:
        {
                ret = default_notify (this, event, data);
        }
        }

        return ret;
}

int32_t
rio_client_init (xlator_t *this)
{
        return rio_common_init (this);
}

void
rio_client_fini (xlator_t *this)
{
        return rio_common_fini (this);
}

class_methods_t class_methods = {
        .init           = rio_client_init,
        .fini           = rio_client_fini,
        .notify         = rio_client_notify,
};

struct xlator_fops fops = {
        .lookup         = rio_client_lookup,
        .stat           = rio_client_stat,
        .stat           = rio_client_stat,
        .fstat          = rio_client_fstat,
        .truncate       = rio_client_truncate,
        .ftruncate      = rio_client_ftruncate,
        .access         = rio_client_access,
        .readlink       = rio_client_readlink,
        .mknod          = rio_client_mknod,
        .mkdir          = rio_client_mkdir,
        .unlink         = rio_client_unlink,
        .rmdir          = rio_client_rmdir,
        .symlink        = rio_client_symlink,
        .rename         = rio_client_rename,
        .link           = rio_client_link,
        .create         = rio_client_create,
        .open           = rio_client_open,
        .readv          = rio_client_readv,
        .writev         = rio_client_writev,
        .flush          = rio_client_flush,
        .fsync          = rio_client_fsync,
        .opendir        = rio_client_opendir,
        .readdir        = rio_client_readdir,
        .readdirp       = rio_client_readdirp,
        .fsyncdir       = rio_client_fsyncdir,
        .statfs         = rio_client_statfs,
        .setxattr       = rio_client_setxattr,
        .getxattr       = rio_client_getxattr,
        .fsetxattr      = rio_client_fsetxattr,
        .fgetxattr      = rio_client_fgetxattr,
        .removexattr    = rio_client_removexattr,
        .fremovexattr   = rio_client_fremovexattr,
        .lk             = rio_client_lk,
        .inodelk        = rio_client_inodelk,
        .finodelk       = rio_client_finodelk,
        .entrylk        = rio_client_entrylk,
        .fentrylk       = rio_client_fentrylk,
        .rchecksum      = rio_client_rchecksum,
        .xattrop        = rio_client_xattrop,
        .fxattrop       = rio_client_fxattrop,
        .setattr        = rio_client_setattr,
        .fsetattr       = rio_client_fsetattr,
        .fallocate      = rio_client_fallocate,
        .discard        = rio_client_discard,
        .zerofill       = rio_client_zerofill,
        .ipc            = rio_client_ipc,
#ifdef HAVE_SEEK_HOLE
        .seek           = rio_client_seek,
#endif
        .lease          = rio_client_lease,
/*        .compound       = rio_client_compound,*/
        .getactivelk    = rio_client_getactivelk,
        .setactivelk    = rio_client_setactivelk,
};

struct xlator_cbks cbks = {
};

/*
struct xlator_dumpops dumpops = {
};
*/
