/*
  Copyright (c) 2017 Red Hat, Inc. <http://www.redhat.com>
  This file is part of GlusterFS.

  This file is licensed to you under your choice of the GNU Lesser
  General Public License, version 3 or any later version (LGPLv3 or
  later), or the GNU General Public License, version 2 (GPLv2), in all
  cases as published by the Free Software Foundation.
*/

/* File: rio-server-ds-fops.c
 * This file contains the RIO DC/server FOP entry and exit points.
 */

#include "xlator.h"

#include "rio-common.h"
#include "rio-common-helpers.h"
#include "rio-server-mds-fops.h"
#include "layout.h"
