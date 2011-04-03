/*
 * userconfig.h -- Read and write the user configuration file
 *
 * Copyright (C) 2007-2009, 2011 Igalia, S.L.
 * Authors: Alberto Garcia <agarcia@igalia.com>
 *
 * This file is part of Vagalume and is published under the GNU GPLv3.
 * See the README file for more details.
 */

#ifndef USERCONFIG_H
#define USERCONFIG_H

#include <glib.h>

#include "vgl-server.h"

typedef struct {
        char *username; /* Never NULL, even if not defined */
        char *password; /* Never NULL, even if not defined */
        char *http_proxy; /* Never NULL, even if not defined */
        char *download_dir; /* Never NULL */
        char *imstatus_template; /* Never NULL */
        VglServer *server;
        gboolean use_proxy;
        gboolean low_bitrate;
        gboolean discovery_mode;
        gboolean enable_scrobbling;
        gboolean im_pidgin;
        gboolean im_gajim;
        gboolean im_gossip;
        gboolean im_telepathy;
        gboolean disable_confirm_dialogs;
        gboolean show_notifications;
        gboolean close_to_systray;
        gboolean autodl_free_tracks;
} VglUserCfg;

VglUserCfg *
vgl_user_cfg_new                        (void);

void
vgl_user_cfg_destroy                    (VglUserCfg *cfg);

void
vgl_user_cfg_set_username               (VglUserCfg *cfg,
                                         const char *username);

void
vgl_user_cfg_set_password               (VglUserCfg *cfg,
                                         const char *password);

void
vgl_user_cfg_set_http_proxy             (VglUserCfg *cfg,
                                         const char *proxy);

void
vgl_user_cfg_set_download_dir           (VglUserCfg *cfg,
                                         const char *dir);

void
vgl_user_cfg_set_server_name            (VglUserCfg *cfg,
                                         const char *name);

void
vgl_user_cfg_set_imstatus_template      (VglUserCfg *cfg,
                                         const char *str);

VglUserCfg *
vgl_user_cfg_read                       (void);

gboolean
vgl_user_cfg_write                      (VglUserCfg *cfg);

const char *
vgl_user_cfg_get_cfgdir                 (void);

#endif
