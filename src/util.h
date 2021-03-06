/*
 * util.h -- Misc util functions
 *
 * Copyright (C) 2007-2009 Igalia, S.L.
 * Authors: Alberto Garcia <berto@igalia.com>
 *
 * This file is part of Vagalume and is published under the GNU GPLv3
 * See the README file for more details.
 */
#ifndef UTIL_H
#define UTIL_H

#include "config.h"
#include "playlist.h"
#include <glib.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <libxml/parser.h>

#ifdef HAVE_GIO
#    include <gio/gio.h>
#endif

char *
get_md5_hash                            (const char *str);

char *
compute_auth_token                      (const char *password,
                                         const char *timestamp);

char *
str_glist_join                          (const GList *list,
                                         const char  *separator);

gboolean
file_exists                             (const char *filename);

void
string_replace_gstr                     (GString    *str,
                                         const char *old,
                                         const char *new);

char *
string_replace                          (const char *str,
                                         const char *old,
                                         const char *new);

GdkPixbuf *
get_pixbuf_from_image                   (const char *data,
                                         size_t      size,
                                         int         imgsize);

const char *
get_home_directory                      (void);

const char *
get_language_code                       (void);

char *
obfuscate_string                        (char *str);

char *
lastfm_url_decode                       (const char *url);

char *
lastfm_url_encode                       (const char *url);

void
xml_add_string                          (xmlNode    *parent,
                                         const char *name,
                                         const char *value);

void
xml_add_bool                            (xmlNode    *parent,
                                         const char *name,
                                         gboolean    value);

void
xml_add_glong                           (xmlNode    *parent,
                                         const char *name,
                                         glong       value);

const xmlNode *
xml_find_node                           (const xmlNode *node,
                                         const char    *name);

const xmlNode *
xml_get_string                          (xmlDoc         *doc,
                                         const xmlNode  *node,
                                         const char     *name,
                                         char          **value);

const xmlNode *
xml_get_bool                            (xmlDoc        *doc,
                                         const xmlNode *node,
                                         const char    *name,
                                         gboolean      *value);

const xmlNode *
xml_get_glong                           (xmlDoc        *doc,
                                         const xmlNode *node,
                                         const char    *name,
                                         glong         *value);

#ifdef HAVE_GIO
void
launch_url                              (const char        *url,
                                         GAppLaunchContext *context);
#endif

void
lastfm_get_track_cover_image            (LastfmTrack *track);

#endif
