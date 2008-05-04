/*
 * userconfig.h -- Read and write the user configuration file
 *
 * Copyright (C) 2007 Alberto Garcia <agarcia@igalia.com>
 *
 * This file is part of Vagalume and is published under the GNU GPLv3.
 * See the README file for more details.
 */

#ifndef USERCONFIG_H
#define USERCONFIG_H

#include <glib.h>

typedef struct {
        char *username; /* Never NULL, even if not defined */
        char *password; /* Never NULL, even if not defined */
        char *http_proxy; /* Never NULL, even if not defined */
        char *download_dir; /* Never NULL */
        char *imstatus_template;
        gboolean use_proxy;
        gboolean discovery_mode;
        gboolean enable_scrobbling;
        gboolean im_pidgin;
        gboolean im_gajim;
        gboolean im_gossip;
        gboolean im_telepathy;
        gboolean disable_confirm_dialogs;
        gboolean show_notifications;
        gboolean close_to_systray;
} lastfm_usercfg;

lastfm_usercfg *lastfm_usercfg_new(void);
void lastfm_usercfg_destroy(lastfm_usercfg *cfg);
void lastfm_usercfg_set_username(lastfm_usercfg *cfg, const char *username);
void lastfm_usercfg_set_password(lastfm_usercfg *cfg, const char *password);
void lastfm_usercfg_set_http_proxy(lastfm_usercfg *cfg, const char *proxy);
void lastfm_usercfg_set_download_dir(lastfm_usercfg *cfg, const char *dir);
void lastfm_usercfg_set_imstatus_template(lastfm_usercfg *cfg, const char *str);
lastfm_usercfg *read_usercfg(void);
gboolean write_usercfg(lastfm_usercfg *cfg);

#endif
