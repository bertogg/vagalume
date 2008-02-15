/*
 * dlwin.h -- Download window
 * Copyright (C) 2007 Alberto Garcia <agarcia@igalia.com>
 *
 * This file is part of Vagalume and is published under the GNU GPLv3
 * See the README file for more details.
 */

#ifndef DLWIN_H
#define DLWIN_H

void dlwin_download_file(const char *url, const char *filename,
                         const char *dstpath);

#endif
