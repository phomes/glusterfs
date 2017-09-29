/*
  Copyright (c) 2016 Red Hat, Inc. <http://www.redhat.com>
  This file is part of GlusterFS.

  This file is licensed to you under your choice of the GNU Lesser
  General Public License, version 3 or any later version (LGPLv3 or
  later), or the GNU General Public License, version 2 (GPLv2), in all
  cases as published by the Free Software Foundation.
*/

/* File: layout.c
 * Provides the layout object abstraction base.
 * Used to extend with different layout types that can govern distribution.
 *
 * TODO:
 *  - Comments in the code
 *  - Test program to test this in isolation
 */

#include "compat-uuid.h"

#include "layout.h"

struct layout_definitions {
        const char *layouttype;
        struct layout_ops *ops;
};

extern struct layout_ops layout_inodehash_bucket;
extern struct layout_ops layout_static_bucket;

#define LAYOUT_MAX_LAYOUTS 2
static struct layout_definitions supported[LAYOUT_MAX_LAYOUTS] = {
        [0] = {LAYOUT_INODEHASH_BUCKET, &layout_inodehash_bucket},
        [1] = {LAYOUT_STATIC_BUCKET, &layout_static_bucket}
};

struct layout *
layout_init (char *layouttype, int count, struct rio_subvol *xlators,
             dict_t *options)
{
        int i;
        struct layout *new_layout = NULL;

        if (!layouttype || !xlators || !options || count <= 0)
                goto out;

        /* Determine if passed in layout type is supported */
        for (i = 0; i < LAYOUT_MAX_LAYOUTS; i++) {
                if (strncmp (supported[i].layouttype, layouttype,
                             strlen(supported[i].layouttype)) == 0) {
                        new_layout =
                                supported[i].ops->laops_init (layouttype, count,
                                                              xlators, options);
                        break;
                }
        }
out:
        return new_layout;
}

xlator_t *
layout_search (struct layout *layout, uuid_t gfid)
{
        if (!layout)
                return NULL;

        return layout->layou_ops->laops_search (layout, gfid);
}

uint32_t
gfid_to_bucket (uuid_t gfid)
{
        uint32_t  bucket = 0;

        bucket += (uint32_t)(gfid[0]) << 8;
        bucket += (uint32_t)(gfid[1]);

        return bucket;
}

/* TODO: Not sure if this is where this abstraction fits in */
void
layout_generate_colocated_gfid (struct layout *searchin, uuid_t out_gfid,
                                uuid_t in_gfid)
{
        int32_t bucket;

        bucket = gfid_to_bucket (in_gfid);
        gf_uuid_generate (out_gfid);

        /* TODO: are we endian safe here? */
        out_gfid[0] = ((bucket >> 8) & 0xFF);
        out_gfid[1] = ((bucket) & 0xFF);
}

/*
int
layout_update (struct layout *layout)
{

}

int
layout_refresh (struct layout *layout)
{

}
*/

void
layout_destroy (struct layout *layout)
{
        if (!layout)
                return;

        layout->layou_ops->laops_destroy (layout);

        return;
}
