/*
  Copyright (c) 2017 Red Hat, Inc. <http://www.redhat.com>
  This file is part of GlusterFS.

  This file is licensed to you under your choice of the GNU Lesser
  General Public License, version 3 or any later version (LGPLv3 or
  later), or the GNU General Public License, version 2 (GPLv2), in all
  cases as published by the Free Software Foundation.
*/

/* File: rio-server-common.h
 * This file contains declarations for all RIO DS/MDS common functions.
 */

#ifndef _RIO_SERVER_COMMON_H
#define _RIO_SERVER_COMMON_H

int
rio_server_notify (xlator_t *, int32_t, void *, ...);

int32_t
rio_server_init (xlator_t *);

void
rio_server_fini (xlator_t *);

#endif /* _RIO_SERVER_COMMON_H */
