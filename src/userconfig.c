/*
 * userconfig.c -- Read and write the user configuration file
 *
 * Copyright (C) 2007 Alberto Garcia <agarcia@igalia.com>
 *
 * This file is part of Vagalume and is published under the GNU GPLv3.
 * See the README file for more details.
 */

#include <stdio.h>
#include <glib/gstdio.h>
#include <string.h>
#include <sys/types.h>
#include <regex.h>
#include <libxml/parser.h>

#include "globaldefs.h"
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

/* Old config file, to be removed soon */
static char *
get_old_cfg_filename(void)
{
        const char *homedir = get_home_directory ();
        return g_strconcat(homedir, "/.vagalumerc", NULL);
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
vgl_user_cfg_get_cfgdir(void)
{
        static char *cfgdir = NULL;
        if (cfgdir == NULL) {
                const char *homedir = get_home_directory ();
                if (homedir != NULL) {
                        cfgdir = g_strconcat (homedir, "/" VAGALUME_CONFIG_DIR,
                                              NULL);
                }
                if (cfgdir != NULL) {
                        g_mkdir_with_parents (cfgdir, 0755);
                }
        }
        return cfgdir;
}

static const char *
get_cfg_filename(void)
{
        static char *cfgfile = NULL;

        if (cfgfile == NULL) {
                const char *cfgdir = vgl_user_cfg_get_cfgdir ();
                cfgfile = g_strconcat (cfgdir, "/config.xml" , NULL);
        }
        return cfgfile;
}

void
vgl_user_cfg_set_username(VglUserCfg *cfg, const char *username)
{
        g_return_if_fail(cfg != NULL && username != NULL);
        g_free(cfg->username);
        cfg->username = g_strstrip(g_strdup(username));
}

void
vgl_user_cfg_set_password(VglUserCfg *cfg, const char *password)
{
        g_return_if_fail(cfg != NULL && password != NULL);
        g_free(cfg->password);
        cfg->password = g_strstrip(g_strdup(password));
}

void
vgl_user_cfg_set_http_proxy(VglUserCfg *cfg, const char *proxy)
{
        g_return_if_fail(cfg != NULL && proxy != NULL);
        g_free(cfg->http_proxy);
        cfg->http_proxy = g_strstrip(g_strdup(proxy));
}

void
vgl_user_cfg_set_download_dir(VglUserCfg *cfg, const char *dir)
{
        g_return_if_fail(cfg != NULL && dir != NULL);
        g_free(cfg->download_dir);
        cfg->download_dir = g_strstrip(g_strdup(dir));
}

void
vgl_user_cfg_set_imstatus_template(VglUserCfg *cfg, const char *str)
{
        g_return_if_fail(cfg != NULL && str != NULL);
        g_free(cfg->imstatus_template);
        cfg->imstatus_template = g_strstrip(g_strdup(str));
}

VglUserCfg *
vgl_user_cfg_new(void)
{
        VglUserCfg *cfg = g_slice_new0(VglUserCfg);
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
        cfg->autodl_free_tracks = FALSE;
        return cfg;
}

void
vgl_user_cfg_destroy(VglUserCfg *cfg)
{
        g_free(cfg->username);
        g_free(cfg->password);
        g_free(cfg->http_proxy);
        g_free(cfg->download_dir);
        g_free(cfg->imstatus_template);
        g_slice_free(VglUserCfg, cfg);
}

static VglUserCfg *
lastfm_old_usercfg_read(void)
{
        VglUserCfg *cfg = NULL;
        const int bufsize = 256;
        char buf[bufsize];
        char *cfgfile;
        FILE *fd = NULL;
        cfgfile = get_old_cfg_filename();
        if (cfgfile != NULL) fd = fopen(cfgfile, "r");
        g_free(cfgfile);
        if (fd == NULL) {
                g_debug("Config file not found");
                return NULL;
        }
        cfg = vgl_user_cfg_new();
        while (fgets(buf, bufsize, fd)) {
                int len = strlen(buf);
                char *val;
                if (len == 0) continue;
                if (buf[len-1] == '\n') buf[len-1] = '\0';
                if ((val = cfg_get_val(buf, "username")) != NULL) {
                        vgl_user_cfg_set_username(cfg, val);
                } else if ((val = cfg_get_val(buf, "password")) != NULL) {
#ifdef MAEMO
                        vgl_user_cfg_set_password(cfg, val);
#else
                        gsize len;
                        char *pw = (char *) g_base64_decode(val, &len);
                        pw = g_realloc(pw, len+1);
                        pw[len] = '\0';
                        vgl_user_cfg_set_password(cfg, pw);
                        g_free(pw);
#endif
                } else if ((val = cfg_get_val(buf, "discovery")) != NULL) {
                        cfg->discovery_mode = !strcmp(val, "1");
                } else if ((val = cfg_get_val(buf, "scrobble")) != NULL) {
                        cfg->enable_scrobbling = !strcmp(val, "1");
                } else if ((val = cfg_get_val(buf, "use_proxy")) != NULL) {
                        cfg->use_proxy = !strcmp(val, "1");
                } else if ((val = cfg_get_val(buf, "http_proxy")) != NULL) {
                        vgl_user_cfg_set_http_proxy(cfg, val);
                } else if ((val = cfg_get_val(buf, "download_dir")) != NULL) {
                        vgl_user_cfg_set_download_dir(cfg, val);
                } else if ((val = cfg_get_val(buf, "imstatus_template")) != NULL) {
                        vgl_user_cfg_set_imstatus_template(cfg, val);
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

static void
xml_add_string (xmlNode *parent, const char *name, const char *value)
{
        xmlNode *node;
        xmlChar *enc;

        node = xmlNewNode (NULL, (xmlChar *) name);
        enc = xmlEncodeEntitiesReentrant (NULL, (xmlChar *) value);
        xmlNodeSetContent (node, enc);
        xmlFree (enc);

        xmlAddChild (parent, node);
}

static void
xml_add_bool (xmlNode *parent, const char *name, gboolean value)
{
        xml_add_string (parent, name, value ? "1" : "0");
}

static void
xml_get_string (xmlDoc *doc, const xmlNode *node,
                const char *name, char **value)
{
        const xmlNode *iter;
        xmlChar *val = NULL;
        gboolean found = FALSE;

        g_return_if_fail (doc && node && name && value);

        for (iter = node; iter != NULL && !found; iter = iter->next) {
                if (!xmlStrcmp (iter->name, (const xmlChar *) name)) {
                        val = xmlNodeListGetString
                                (doc, iter->xmlChildrenNode, 1);
                        found = TRUE;
                }

        }

        g_free (*value);

        if (val != NULL) {
                *value = g_strstrip (g_strdup ((char *) val));
                xmlFree (val);
        } else {
                *value = g_strdup ("");
        }

}

static void
xml_get_bool (xmlDoc *doc, const xmlNode *node,
              const char *name, gboolean *value)
{
        char *strval = NULL;

        g_return_if_fail (value != NULL);

        xml_get_string (doc, node, name, &strval);
        *value = g_str_equal (strval, "1");
        g_free (strval);
}

VglUserCfg *
vgl_user_cfg_read (void)
{
        xmlDoc *doc = NULL;
        xmlNode *root = NULL;
        xmlNode *node = NULL;
        VglUserCfg *cfg = NULL;
        const char *cfgfile = get_cfg_filename ();

        if (file_exists (cfgfile)) {
                doc = xmlParseFile (cfgfile);
                if (doc == NULL) {
                        g_warning ("Config file is not a valid XML file");
                }
        } else {
                /* Read old cfg file if the new one doesn't exist */
                g_debug ("Config file not found. Trying old config file.");
                cfg = lastfm_old_usercfg_read ();
                if (cfg != NULL) {
                        g_debug ("Converting old config file to new one.");
                        if (vgl_user_cfg_write (cfg)) {
                                char *oldcfg = get_old_cfg_filename ();
                                g_unlink (oldcfg);
                                g_free (oldcfg);
                        }
                }
        }

        /* Read the config file and do some basic checks */
        if (doc != NULL) {
                root = xmlDocGetRootElement (doc);
                xmlChar *version = xmlGetProp (root, (xmlChar *) "version");
                if (version == NULL ||
                    xmlStrcmp (root->name, (const xmlChar *) "config")) {
                        g_warning ("Error parsing config file");
                } else if (xmlStrcmp (version, (const xmlChar *) "1")) {
                        g_warning ("This config file is version %s, but "
                                   "Vagalume " APP_VERSION " can only "
                                   "read version 1", version);
                } else {
                        node = root->xmlChildrenNode;
                }
                if (version != NULL) xmlFree (version);
        }

        /* Parse the configuration */
        if (node != NULL) {
                cfg = vgl_user_cfg_new();
                /* This code is not very optimal, but it is simpler */
                xml_get_string (doc, node, "username", &(cfg->username));
                xml_get_string (doc, node, "password", &(cfg->password));
                obfuscate_string (cfg->password);
                xml_get_string (doc, node, "http-proxy", &(cfg->http_proxy));
                xml_get_string (doc, node, "download-dir",
                                &(cfg->download_dir));
                xml_get_string (doc, node, "imstatus-template",
                                &(cfg->imstatus_template));
                xml_get_bool (doc, node, "use-proxy", &(cfg->use_proxy));
                xml_get_bool (doc, node, "discovery-mode",
                              &(cfg->discovery_mode));
                xml_get_bool (doc, node, "enable-scrobbling",
                              &(cfg->enable_scrobbling));
                xml_get_bool (doc, node, "im-pidgin", &(cfg->im_pidgin));
                xml_get_bool (doc, node, "im-gajim", &(cfg->im_gajim));
                xml_get_bool (doc, node, "im-gossip", &(cfg->im_gossip));
                xml_get_bool (doc, node, "im-telepathy", &(cfg->im_telepathy));
                xml_get_bool (doc, node, "disable-confirm-dialogs",
                              &(cfg->disable_confirm_dialogs));
                xml_get_bool (doc, node, "show-notifications",
                              &(cfg->show_notifications));
                xml_get_bool (doc, node, "close-to-systray",
                              &(cfg->close_to_systray));
                xml_get_bool (doc, node, "autodownload-free-tracks",
                              &(cfg->autodl_free_tracks));
        }

        if (doc != NULL) xmlFreeDoc (doc);

        return cfg;
}

gboolean
vgl_user_cfg_write (VglUserCfg *cfg)
{
        xmlDoc *doc;
        xmlNode *root;
        gboolean retvalue = TRUE;

        const char *cfgfile = get_cfg_filename ();

        g_return_val_if_fail (cfg != NULL, FALSE);

        doc = xmlNewDoc ((xmlChar *) "1.0");
        root = xmlNewNode (NULL, (xmlChar *) "config");
        xmlSetProp (root, (xmlChar *) "version", (xmlChar *) "1");
        xmlSetProp (root, (xmlChar *) "revision", (xmlChar *) "2");
        xmlDocSetRootElement (doc, root);

        xml_add_string (root, "username", cfg->username);
        xml_add_string (root, "password", obfuscate_string (cfg->password));
        obfuscate_string (cfg->password);
        xml_add_string (root, "http-proxy", cfg->http_proxy);
        xml_add_string (root, "download-dir", cfg->download_dir);
        xml_add_string (root, "imstatus-template", cfg->imstatus_template);
        xml_add_bool (root, "use-proxy", cfg->use_proxy);
        xml_add_bool (root, "discovery-mode", cfg->discovery_mode);
        xml_add_bool (root, "enable-scrobbling", cfg->enable_scrobbling);
        xml_add_bool (root, "im-pidgin", cfg->im_pidgin);
        xml_add_bool (root, "im-gajim", cfg->im_gajim);
        xml_add_bool (root, "im-gossip", cfg->im_gossip);
        xml_add_bool (root, "im-telepathy", cfg->im_telepathy);
        xml_add_bool (root, "disable-confirm-dialogs",
                      cfg->disable_confirm_dialogs);
        xml_add_bool (root, "show-notifications", cfg->show_notifications);
        xml_add_bool (root, "close-to-systray", cfg->close_to_systray);
        xml_add_bool (root, "autodownload-free-tracks",
                      cfg->autodl_free_tracks);

        if (xmlSaveFormatFileEnc (cfgfile, doc, "UTF-8", 1) == -1) {
                g_critical ("Unable to open %s", cfgfile);
                retvalue = FALSE;
        }

        xmlFreeDoc (doc);
        return retvalue;
}
