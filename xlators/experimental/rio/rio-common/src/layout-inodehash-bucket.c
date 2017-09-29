/*
  Copyright (c) 2016 Red Hat, Inc. <http://www.redhat.com>
  This file is part of GlusterFS.

  This file is licensed to you under your choice of the GNU Lesser
  General Public License, version 3 or any later version (LGPLv3 or
  later), or the GNU General Public License, version 2 (GPLv2), in all
  cases as published by the Free Software Foundation.
*/

/* File: layout-inodehash-bucket.c
 * inode hash bucket layout, hashes the search parameter and uses the top 2
 * bytes to get a bucket index  .
 */

#include <stdlib.h>

#include "common-utils.h"

#include "rio-mem-types.h"
#include "rio-common.h"
#include "layout.h"

#define MAX_BUCKETS 65536

struct layout_ops layout_inodehash_bucket;

struct layout *
layout_inodehash_init (char *layouttype, int count, struct rio_subvol *xlators,
                       dict_t *options)
{
        int i;
        struct layout *new_layout = NULL;

        if (!layouttype || !xlators || !options || count <= 0)
                goto out;

        if (strncmp (LAYOUT_INODEHASH_BUCKET, layouttype,
                     strlen(LAYOUT_INODEHASH_BUCKET)) != 0) {
                goto out;
        }

        new_layout = GF_CALLOC (1, sizeof (struct layout), gf_rio_mt_layout);
        if (!new_layout)
                goto out;

        new_layout->layou_ops = &layout_inodehash_bucket;

        /* allocate a contiguous (array) of xlator_t pointers */
        new_layout->layou_buckets = GF_CALLOC (MAX_BUCKETS, sizeof (xlator_t *),
                                         gf_rio_mt_layout_inodehash_bucket);
        if (!new_layout->layou_buckets) {
                GF_FREE (new_layout);
                new_layout = NULL;
                goto out;
        }

        for (i = 0; i < MAX_BUCKETS;) {
                struct rio_subvol *subvol = NULL;

                list_for_each_entry (subvol, &xlators->riosvl_node,
                                     riosvl_node) {
                        new_layout->layou_buckets[i] = subvol->riosvl_xlator;
                        i++;
                        if (i >= MAX_BUCKETS)
                                break;
                }
        }
#ifdef RIO_DEBUG
        {
                for (i = 0; i < (MAX_BUCKETS < 16 ? MAX_BUCKETS : 16); i++)
                        gf_msg ("layout", GF_LOG_INFO, 0, 0,
                                "InodeHashBucket: %d %s",
                                i, new_layout->layou_buckets[i]->name);
        }
#endif

out:
        return new_layout;
}

void
layout_inodehash_destroy (struct layout *layout)
{
        GF_FREE (layout->layou_buckets);
        GF_FREE (layout);

        return;
}

xlator_t *
layout_inodehash_search (struct layout *layout, uuid_t gfid)
{
        uint32_t bucket = 0;
        char xxh64[GF_XXH64_DIGEST_LENGTH*2+1] = {0,};

        /* TODO: write a wrapper that just returns the 64 bit hash, than the
        current double conversion */
        gf_xxh64_wrapper ((unsigned char *)gfid,
                          sizeof(uuid_t), GF_XXHSUM64_DEFAULT_SEED, xxh64);

        /* take the top 2 bytes, IOW 4 chars from the hash for the bucket */
        xxh64[4] = '\0';
        bucket = strtol(xxh64, NULL, 16);

        return layout->layou_buckets[bucket];
}

struct layout_ops layout_inodehash_bucket = {
        .laops_init = layout_inodehash_init,
        .laops_destroy = layout_inodehash_destroy,
        .laops_search = layout_inodehash_search
};
