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

#include "xlator.h"
#include "locking.h"
#include "defaults.h"

#include "rio-common.h"
#include "rio-client-autogen-fops.h"
#include "layout.h"

/* LOOKUP Call
Handle the EREMOTE error, in case the name and inode for the name are on
different MDS subvolumes.

Other than the above special case, no other handling is required in LOOKUP.
*/
int32_t rio_client_lookup_cbk (call_frame_t *frame, void *cookie,
                               xlator_t *this, int32_t op_ret, int32_t op_errno,
                               inode_t *inode, struct iatt *buf, dict_t *xdata,
                               struct iatt *postparent)
{
        STACK_UNWIND_STRICT (lookup, frame, op_ret, op_errno, inode, buf,
                             xdata, postparent);
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

        VALIDATE_OR_GOTO (this, error);
        conf = this->private;
        VALIDATE_OR_GOTO (conf, error);

        subvol = layout_search (conf->riocnf_mdclayout, loc->gfid);

#ifdef RIO_DEBUG
        gf_msg (this->name, GF_LOG_INFO, 0, 0,
                "Sending LOOKUP to subvol %s", subvol->name);
#endif

        STACK_WIND (frame, rio_client_lookup_cbk, subvol, subvol->fops->lookup,
                    loc, xdata);
        return 0;

error:
        STACK_UNWIND (lookup, frame, -1, errno, NULL, NULL, NULL, NULL);
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
