/*
  Copyright (c) 2016 Red Hat, Inc. <http://www.redhat.com>
  This file is part of GlusterFS.

  This file is licensed to you under your choice of the GNU Lesser
  General Public License, version 3 or any later version (LGPLv3 or
  later), or the GNU General Public License, version 2 (GPLv2), in all
  cases as published by the Free Software Foundation.
*/

/* File: rio-server-mds-main.c
 * This file contains the xlator methods and structure definitions.
 */

#include "xlator.h"

#include "rio-common.h"
#include "rio-server-common.h"
#include "rio-server-mds-fops.h"

class_methods_t class_methods = {
        .init           = rio_server_init,
        .fini           = rio_server_fini,
        .notify         = rio_server_notify,
};

struct xlator_fops fops = {
        .lookup         = rio_server_lookup,
        .create         = rio_server_create,
        .mkdir          = rio_server_mkdir
};

struct xlator_cbks cbks = {
};

/*
struct xlator_dumpops dumpops = {
};
*/
