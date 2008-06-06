/*
 * userconfig.c -- Read and write the user configuration file
 *
 * Copyright (C) 2007 Alberto Garcia <agarcia@igalia.com>
 *
 * This file is part of Vagalume and is published under the GNU GPLv3.
 * See the README file for more details.
 */

#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <regex.h>

#include "config.h"
#include "userconfig.h"
#include "util.h"

#define DEFAULT_IMSTATUS_TEMPLATE \
        "\342\231\253 {artist} - {title} \342\231\253 (Vagalume {version})"

static char *
cfg_get_val(const char *line, const char *key)
{
        g_return_val_if_fail(line != NULL && key != NULL, NULL);
        regex_t creg;
        regmatch_t pmatch[2];
        char *value = NULL;
        char *regex;
        regex = g_strconcat("^ *", key, " *= *\"\\(.*\\)\" *$", NULL);
        int comp = regcomp(&creg, regex, 0);
        g_free(regex);
        g_return_val_if_fail(comp == 0, NULL);
        if (regexec(&creg, line, 2, pmatch, 0) == 0) {
                regoff_t first = pmatch[1].rm_so;
                regoff_t last = pmatch[1].rm_eo;
                value = g_strndup(line+first, last-first);
        }
        regfree(&creg);
        return value;
}

static char *
get_cfg_filename(void)
{
        const char *homedir = get_home_directory ();
        return g_strconcat(homedir, "/" VAGALUME_CONF_FILE, NULL);
}

static char *
default_download_dir(void)
{
        const char *homedir = get_home_directory ();
        char *dldir = NULL;
        if (homedir == NULL) {
                return g_strdup("/tmp");
        }
#ifdef MAEMO
        dldir = g_strconcat(homedir, "/MyDocs/.sounds", NULL);
#else
        dldir = g_strdup(homedir);
#endif
        return dldir;
}

const char *
lastfm_usercfg_get_cfgdir(void)
{
        static char *cfgdir = NULL;
        if (cfgdir == NULL) {
                const char *homedir = get_home_directory ();
                if (homedir != NULL) {
                        cfgdir = g_strconcat (homedir, "/.vagalume", NULL);
                }
                if (cfgdir != NULL) {
                        g_mkdir_with_parents (cfgdir, 0755);
                }
        }
        return cfgdir;
}

void
lastfm_usercfg_set_username(lastfm_usercfg *cfg, const char *username)
{
        g_return_if_fail(cfg != NULL && username != NULL);
        g_free(cfg->username);
        cfg->username = g_strstrip(g_strdup(username));
}

void
lastfm_usercfg_set_password(lastfm_usercfg *cfg, const char *password)
{
        g_return_if_fail(cfg != NULL && password != NULL);
        g_free(cfg->password);
        cfg->password = g_strstrip(g_strdup(password));
}

void
lastfm_usercfg_set_http_proxy(lastfm_usercfg *cfg, const char *proxy)
{
        g_return_if_fail(cfg != NULL && proxy != NULL);
        g_free(cfg->http_proxy);
        cfg->http_proxy = g_strstrip(g_strdup(proxy));
}

void
lastfm_usercfg_set_download_dir(lastfm_usercfg *cfg, const char *dir)
{
        g_return_if_fail(cfg != NULL && dir != NULL);
        g_free(cfg->download_dir);
        cfg->download_dir = g_strstrip(g_strdup(dir));
}

void
lastfm_usercfg_set_imstatus_template(lastfm_usercfg *cfg, const char *str)
{
        g_return_if_fail(cfg != NULL && str != NULL);
        g_free(cfg->imstatus_template);
        cfg->imstatus_template = g_strstrip(g_strdup(str));
}

lastfm_usercfg *
lastfm_usercfg_new(void)
{
        lastfm_usercfg *cfg = g_slice_new0(lastfm_usercfg);
        cfg->username = g_strdup("");
        cfg->password = g_strdup("");
        cfg->http_proxy = g_strdup("");
        cfg->download_dir = default_download_dir();
        cfg->imstatus_template = g_strdup(DEFAULT_IMSTATUS_TEMPLATE);
        cfg->use_proxy = FALSE;
        cfg->enable_scrobbling = TRUE;
        cfg->discovery_mode = FALSE;
        cfg->im_pidgin = FALSE;
        cfg->im_gajim = FALSE;
        cfg->im_gossip = FALSE;
        cfg->im_telepathy = FALSE;
        cfg->disable_confirm_dialogs = FALSE;
        cfg->show_notifications = TRUE;
        cfg->close_to_systray = TRUE;
        return cfg;
}

void
lastfm_usercfg_destroy(lastfm_usercfg *cfg)
{
        g_free(cfg->username);
        g_free(cfg->password);
        g_free(cfg->http_proxy);
        g_free(cfg->download_dir);
        g_free(cfg->imstatus_template);
        g_slice_free(lastfm_usercfg, cfg);
}

lastfm_usercfg *
lastfm_usercfg_read(void)
{
        lastfm_usercfg *cfg = NULL;
        const int bufsize = 256;
        char buf[bufsize];
        char *cfgfile;
        FILE *fd = NULL;
        cfgfile = get_cfg_filename();
        if (cfgfile != NULL) fd = fopen(cfgfile, "r");
        g_free(cfgfile);
        if (fd == NULL) {
                g_debug("Config file not found");
                return NULL;
        }
        cfg = lastfm_usercfg_new();
        while (fgets(buf, bufsize, fd)) {
                int len = strlen(buf);
                char *val;
                if (len == 0) continue;
                if (buf[len-1] == '\n') buf[len-1] = '\0';
                if ((val = cfg_get_val(buf, "username")) != NULL) {
                        lastfm_usercfg_set_username(cfg, val);
                } else if ((val = cfg_get_val(buf, "password")) != NULL) {
#ifdef MAEMO
                        lastfm_usercfg_set_password(cfg, val);
#else
                        gsize len;
                        char *pw = (char *) g_base64_decode(val, &len);
                        pw = g_realloc(pw, len+1);
                        pw[len] = '\0';
                        lastfm_usercfg_set_password(cfg, pw);
                        g_free(pw);
#endif
                } else if ((val = cfg_get_val(buf, "discovery")) != NULL) {
                        cfg->discovery_mode = !strcmp(val, "1");
                } else if ((val = cfg_get_val(buf, "scrobble")) != NULL) {
                        cfg->enable_scrobbling = !strcmp(val, "1");
                } else if ((val = cfg_get_val(buf, "use_proxy")) != NULL) {
                        cfg->use_proxy = !strcmp(val, "1");
                } else if ((val = cfg_get_val(buf, "http_proxy")) != NULL) {
                        lastfm_usercfg_set_http_proxy(cfg, val);
                } else if ((val = cfg_get_val(buf, "download_dir")) != NULL) {
                        lastfm_usercfg_set_download_dir(cfg, val);
                } else if ((val = cfg_get_val(buf, "imstatus_template")) != NULL) {
                        lastfm_usercfg_set_imstatus_template(cfg, val);
                } else if ((val = cfg_get_val(buf, "im_pidgin")) != NULL) {
                        cfg->im_pidgin = !strcmp(val, "1");
                } else if ((val = cfg_get_val(buf, "im_gajim")) != NULL) {
                        cfg->im_gajim = !strcmp(val, "1");
                } else if ((val = cfg_get_val(buf, "im_gossip")) != NULL) {
                        cfg->im_gossip = !strcmp(val, "1");
                } else if ((val = cfg_get_val(buf, "im_telepathy")) != NULL) {
                        cfg->im_telepathy = !strcmp(val, "1");
                } else if ((val = cfg_get_val(buf, "disable_confirm_dialogs")) != NULL) {
                        cfg->disable_confirm_dialogs = !strcmp(val, "1");
                } else if ((val = cfg_get_val(buf, "show_notifications")) != NULL) {
                        cfg->show_notifications = !strcmp(val, "1");
                } else if ((val = cfg_get_val(buf, "close_to_systray")) != NULL) {
                        cfg->close_to_systray = !strcmp(val, "1");
                }
                g_free(val);
        }
        fclose(fd);
        return cfg;
}

gboolean
lastfm_usercfg_write(lastfm_usercfg *cfg)
{
        g_return_val_if_fail(cfg, FALSE);
        gboolean retval = TRUE;
        char *cfgfile, *base64pw;
        FILE *fd = NULL;
        cfgfile = get_cfg_filename();
        if (cfgfile != NULL) fd = fopen(cfgfile, "w");
        g_free(cfgfile);
        if (fd == NULL) {
                g_warning("Unable to write config file");
                return FALSE;
        }
#ifdef MAEMO
        base64pw = g_strdup(cfg->password);
#else
        base64pw = g_base64_encode((guchar *)cfg->password,
                                   MAX(1,strlen(cfg->password)));
#endif
        if (fprintf(fd, "username=\"%s\"\npassword=\"%s\"\n"
                    "http_proxy=\"%s\"\nuse_proxy=\"%d\"\n"
                    "scrobble=\"%d\"\ndiscovery=\"%d\"\n"
                    "download_dir=\"%s\"\n"
                    "imstatus_template=\"%s\"\n"
                    "im_pidgin=\"%d\"\n"
                    "im_gajim=\"%d\"\n"
                    "im_gossip=\"%d\"\n"
                    "im_telepathy=\"%d\"\n"
                    "disable_confirm_dialogs=\"%d\"\n"
                    "show_notifications=\"%d\"\n"
                    "close_to_systray=\"%d\"\n",
                    cfg->username, base64pw, cfg->http_proxy,
                    !!cfg->use_proxy, !!cfg->enable_scrobbling,
                    !!cfg->discovery_mode,
                    cfg->download_dir,
                    cfg->imstatus_template,
                    !!cfg->im_pidgin,
                    !!cfg->im_gajim,
                    !!cfg->im_gossip,
                    !!cfg->im_telepathy,
                    !!cfg->disable_confirm_dialogs,
                    !!cfg->show_notifications,
                    !!cfg->close_to_systray) <= 0) {
                g_warning("Error writing to config file");
                retval = FALSE;
        }
        g_free(base64pw);
        fclose(fd);
        return retval;
}
