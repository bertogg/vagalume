/*
 * dlwin.h -- Download window
 * Copyright (C) 2007 Alberto Garcia <agarcia@igalia.com>
 *
 * This file is part of Vagalume and is published under the GNU GPLv3
 * See the README file for more details.
 */

#ifndef DLWIN_H
#define DLWIN_H

typedef void (*dlwin_cb)(gboolean success, gpointer userdata);

void dlwin_download_file(const char *url, const char *filename,
                         const char *dstpath, dlwin_cb cb, gpointer cbdata);

#endif
