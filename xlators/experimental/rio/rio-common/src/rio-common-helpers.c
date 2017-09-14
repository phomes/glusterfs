/*
  Copyright (c) 2016 Red Hat, Inc. <http://www.redhat.com>
  This file is part of GlusterFS.

  This file is licensed to you under your choice of the GNU Lesser
  General Public License, version 3 or any later version (LGPLv3 or
  later), or the GNU General Public License, version 2 (GPLv2), in all
  cases as published by the Free Software Foundation.
*/

/* File: rio-common-helpers.c
 * This file contains common rio helper routines
 */

#include "glusterfs.h"
#include "rio-common-helpers.h"

/* This function is a simple node allocator and appending
it to the given list */
int
rio_add_subvol_child (xlator_t *child, struct list_head *head)
{
        int ret = -1;
        struct rio_subvol *newnode;

        newnode = GF_CALLOC (1, sizeof (struct rio_subvol),
                             gf_rio_mt_rio_subvol);
        if (!newnode) {
                goto out;
        }

        newnode->riosvl_xlator = child;
        list_add_tail (&newnode->riosvl_node, head);

        ret = 0;
out:
        return ret;
}

/* IN:
        this: the current xlator
        subvol_str: String of the format "subvol_name:subvol_name:..."
   OUT:
        head populated with parsed subvol information as linked
        struct rio_subvol nodes
        ret is the number of subvolumes that were parsed, -1 on errors

This function parses the subvol string, and finds the appropriate subvol from
the passed in xlators children, and populates the struct rio_subvol
appropriately and inserts it into the passed in list.

errno is set appropriately.
No in args check, all values are assumed to be non-NULL
error return will not cleanup partial work done
*/
int
rio_create_subvol_list (xlator_t *this, char *subvol_str,
                        struct list_head *head)
{
        int ret = 0, count = 0, first_done = 0;
        char *svstr_copy;
        char *subvol, *next_subvol;
        char *saveptr, *first_subvol = NULL;
        xlator_t *child, *first_child = NULL;
        xlator_list_t *children;

        svstr_copy = gf_strdup (subvol_str);
        if (!svstr_copy) {
                errno = ENOMEM;
                return -1;
        }

        /* TODO: HACK
        Currently sorting the given subvol list is a hack, so that we get
        idempotent layouts on all clients and servers. Ideally, we need a
        subvol ID, that can be stored against a layout, in the absence of
        that, and as we create hand written vol files, to get idempotent
        layouts, we sort the subvol names, and name the server side subvol
        the same as the client side subvols for rio client and server. Further
        the server side subvol list is in the same order as the client except
        for the first subvol.
        This hack is handled in storing and using first_subvol/child */
        for (subvol = strtok_r (svstr_copy, RIO_SUBVOL_STRING_SEP, &saveptr);
                subvol; subvol = next_subvol) {
                next_subvol = strtok_r (NULL, RIO_SUBVOL_STRING_SEP, &saveptr);

                /* find subvol in current children */
                for (children = this->children; children;
                     children = children->next) {
                        child = children->xlator;
                        if (strcmp (subvol, child->name) == 0) {
                                if (!first_subvol) {
                                        first_subvol = subvol;
                                        first_child = child;
                                        break;
                                }

                                if (!first_done &&
                                    strcmp(first_subvol, subvol) < 0) {
                                        ret = rio_add_subvol_child (
                                                first_child, head);
                                        if (ret)
                                                goto out;
                                        count++;
                                        first_done = 1;
                                }
                                /* add to list and update found count */
                                ret = rio_add_subvol_child (child, head);
                                if (ret)
                                        goto out;
                                count++;
                                break;
                        }
                }
                /* TODO: If subvol is not found in the list, then error out */
        }

        if (!first_done) {
                /* add to list and update found count */
                ret = rio_add_subvol_child (first_child, head);
                if (ret)
                        goto out;
                count++;
        }
out:
        GF_FREE (svstr_copy);
        ret = ret ? ret : count;
        return ret;
}

/* IN:
        this: the current xlator
        conf: rio_conf structure, with the subvolume strings
   OUT:
        conf: populated with the subvolumes list
        ret: 0 on success, -1 on errors

This function populates rio conf with the subvolume xlator lists
split between the DC and the MDC as appropriate. It requires that the
subvolumes strings are already populated in the conf.

errno is set on appropriately
No args check, expects arguments are non-NULL
On error no cleanup is done, and potentially partially filled data is returned
*/
int
rio_process_volume_lists (xlator_t *this, struct rio_conf *conf)
{
        int ret;

        ret = rio_create_subvol_list (this, conf->riocnf_data_subvolumes,
                                      &conf->riocnf_dc_list.riosvl_node);
        if (ret == -1 || ret == 0) {
                /* no volumes in either data or metadata lists is an error */
                ret = -1;
                goto out;
        } else {
                conf->riocnf_dc_count = ret;
                ret = 0;
        }

        ret = rio_create_subvol_list (this, conf->riocnf_metadata_subvolumes,
                                      &conf->riocnf_mdc_list.riosvl_node);
        if (ret == -1 || ret == 0) {
                /* no volumes in either data or metadata lists is an error */
                ret = -1;
                goto out;
        } else {
                conf->riocnf_mdc_count = ret;
                ret = 0;
        }

        /* TODO: check that the subvolumes are mutually exclusive */
out:
        return ret;
}

void
rio_destroy_volume_lists (struct rio_conf *conf)
{
        struct rio_subvol *node, *tmp;


        list_for_each_entry_safe (node, tmp,
                                  &conf->riocnf_dc_list.riosvl_node,
                                  riosvl_node) {
                list_del_init (&node->riosvl_node);
                GF_FREE (node);
        }

        list_for_each_entry_safe (node, tmp,
                                  &conf->riocnf_mdc_list.riosvl_node,
                                  riosvl_node) {
                list_del_init (&node->riosvl_node);
                GF_FREE (node);
        }

        return;
}

int32_t
rio_lookup_is_nameless (loc_t *loc)
{
        /* pargfid is NULL, then there is no parent under which to lookup the
        name, hence it is a nameless lookup and gfid should exist in loc.
        If there is no name, then again it is a nameless lookup and gfid should
        exist in loc */
        return (gf_uuid_is_null (loc->pargfid) || !loc->name);
}

struct rio_local *
rio_local_init (call_frame_t *frame, struct rio_conf *conf, loc_t *loc,
                fd_t *fd, dict_t *xdata, glusterfs_fop_t fop)
{
        struct rio_local *local;
        int ret;

        /* TODO: consider a mem pool for this, mem_pool_new */
        local = GF_CALLOC (1, sizeof (struct rio_local), gf_rio_mt_local);
        if (!local)
                goto error;

        if (loc) {
                ret = loc_copy (&local->riolocal_loc, loc);
                if (ret)
                        goto cleanup;
        }

        if (fd)
                local->riolocal_fd = fd_ref (fd);

        if (xdata)
                local->riolocal_xdata_in = dict_ref (xdata);

        local->riolocal_fop = fop;

        frame->local = local;

        return local;
cleanup:
        GF_FREE (local);
error:
        return NULL;
}

void
rio_local_wipe (struct rio_local *local)
{
        if (!local)
                return;

        loc_wipe (&local->riolocal_loc);

        if (local->riolocal_fd)
                fd_unref (local->riolocal_fd);

        if (local->riolocal_xdata_in)
                dict_unref (local->riolocal_xdata_in);

        if (local->riolocal_inode)
                inode_unref (local->riolocal_inode);

        if (local->riolocal_xdata_out)
                dict_unref (local->riolocal_xdata_out);

        GF_FREE (local);
}

void
rio_prepare_inode_loc (loc_t *dst, inode_t *inode, uuid_t gfid,
                       gf_boolean_t auxparent)
{
        /* Set the gfid we want to work with */
        gf_uuid_copy (dst->gfid, gfid);

        /* retain the inode, it all belongs togeather */
        if (inode)
                dst->inode = inode_ref (inode);

        if (auxparent) {
                /* Set the parent to the ausx GFID */
                memset (dst->pargfid, 0, sizeof (uuid_t));
                dst->pargfid[15] = GF_AUXILLARY_PARGFID;
        }

        return;
}

#define set_if_greater(a, b) do {               \
                if ((a) < (b))                  \
                        (a) = (b);              \
        } while (0)


#define set_if_greater_time(a, an, b, bn) do {                          \
                if (((a) < (b)) || (((a) == (b)) && ((an) < (bn)))) {   \
                        (a) = (b);                                      \
                        (an) = (bn);                                    \
                }                                                       \
        } while (0)                                                     \

int
rio_iatt_merge (struct iatt *to, struct iatt *from)
{
        if (!from || !to)
                return 0;

        to->ia_dev      = from->ia_dev;

        gf_uuid_copy (to->ia_gfid, from->ia_gfid);

        to->ia_ino      = from->ia_ino;
        to->ia_prot     = from->ia_prot;
        to->ia_type     = from->ia_type;
        to->ia_nlink    = from->ia_nlink;
        to->ia_rdev     = from->ia_rdev;
        to->ia_size    += from->ia_size;
        to->ia_blksize  = from->ia_blksize;
        to->ia_blocks  += from->ia_blocks;

        set_if_greater (to->ia_uid, from->ia_uid);
        set_if_greater (to->ia_gid, from->ia_gid);

        set_if_greater_time(to->ia_atime, to->ia_atime_nsec,
                            from->ia_atime, from->ia_atime_nsec);
        set_if_greater_time (to->ia_mtime, to->ia_mtime_nsec,
                             from->ia_mtime, from->ia_mtime_nsec);
        set_if_greater_time (to->ia_ctime, to->ia_ctime_nsec,
                             from->ia_ctime, from->ia_ctime_nsec);

        return 0;
}

int
rio_iatt_copy (struct iatt *to, struct iatt *from)
{
        if (!from || !to)
                return 0;

        to->ia_dev      = from->ia_dev;

        gf_uuid_copy (to->ia_gfid, from->ia_gfid);

        to->ia_ino        = from->ia_ino;
        to->ia_prot       = from->ia_prot;
        to->ia_type       = from->ia_type;
        to->ia_nlink      = from->ia_nlink;
        to->ia_rdev       = from->ia_rdev;
        to->ia_size       = from->ia_size;
        to->ia_blksize    = from->ia_blksize;
        to->ia_blocks     = from->ia_blocks;

        to->ia_uid        = from->ia_uid;
        to->ia_gid        = from->ia_gid;
        to->ia_atime      = from->ia_atime;
        to->ia_atime_nsec = from->ia_atime_nsec;
        to->ia_mtime      = from->ia_mtime;
        to->ia_mtime_nsec = from->ia_mtime_nsec;
        to->ia_ctime      = from->ia_ctime;
        to->ia_ctime_nsec = from->ia_ctime_nsec;

        return 0;
}

int
rio_iatt_copy_mds (struct iatt *to, struct iatt *from)
{
        if (!from || !to)
                return 0;

        to->ia_dev      = from->ia_dev;

        gf_uuid_copy (to->ia_gfid, from->ia_gfid);

        to->ia_ino        = from->ia_ino;
        to->ia_prot       = from->ia_prot;
        to->ia_type       = from->ia_type;
        to->ia_nlink      = from->ia_nlink;
        to->ia_rdev       = from->ia_rdev;
        to->ia_blksize    = from->ia_blksize;
        /*
         * to->ia_size       = from->ia_size;
         * to->ia_blocks     = from->ia_blocks;
         */

        to->ia_uid        = from->ia_uid;
        to->ia_gid        = from->ia_gid;
        /*
         to->ia_atime      = from->ia_atime;
         to->ia_atime_nsec = from->ia_atime_nsec;
         to->ia_mtime      = from->ia_mtime;
         to->ia_mtime_nsec = from->ia_mtime_nsec;
         */
        to->ia_ctime      = from->ia_ctime;
        to->ia_ctime_nsec = from->ia_ctime_nsec;

        return 0;
}
