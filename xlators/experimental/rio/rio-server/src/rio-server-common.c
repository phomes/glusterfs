/*
  Copyright (c) 2017 Red Hat, Inc. <http://www.redhat.com>
  This file is part of GlusterFS.

  This file is licensed to you under your choice of the GNU Lesser
  General Public License, version 3 or any later version (LGPLv3 or
  later), or the GNU General Public License, version 2 (GPLv2), in all
  cases as published by the Free Software Foundation.
*/

/* File: rio-server-common.c
 * This file contains the common xlator functions, for both RIO DS and MDS
 * library instances.
 */

#include "xlator.h"

#include "rio-common.h"
#include "defaults.h"
#include "rio-server-common.h"

int
rio_server_notify (xlator_t *this, int32_t event, void *data, ...)
{
        int ret = 0;

        /* TODO: Pretty darn unsure about this!
        Need to check this better and see if the following actions are right */
        switch (event) {
        /*case GF_EVENT_CHILD_CONNECTING:*/
        case GF_EVENT_SOME_DESCENDENT_DOWN:
        case GF_EVENT_SOME_DESCENDENT_UP:
        case GF_EVENT_CHILD_DOWN:
        case GF_EVENT_CHILD_UP:
        {
                xlator_t *subvol = data;
                struct rio_conf *conf = this->private;

                /* send local child events only to parents */
                if (!strcmp (subvol->name, conf->riocnf_server_local_subvol)) {
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
rio_server_init (xlator_t *this)
{
        return rio_common_init (this);
}

void
rio_server_fini (xlator_t *this)
{
        return rio_common_fini (this);
}
