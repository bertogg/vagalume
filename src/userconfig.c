
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <regex.h>

#include "userconfig.h"

#define CONFIG_FILE ".lastfmrc"

static char *
cfg_get_val(const char *line, const char *key)
{
        g_return_val_if_fail(line != NULL && key != NULL, NULL);
        regex_t creg;
        regmatch_t pmatch[2];
        char *value = NULL;
        char *regex;
        regex = g_strconcat("^ *", key, " *= *\"\\([^\"]*\\)\" *$", NULL);
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
        const char *homedir = getenv("HOME");
        if (homedir == NULL) {
                g_warning("HOME environment variable not set");
                return NULL;
        }
        return g_strconcat(homedir, "/" CONFIG_FILE, NULL);
}

void
lastfm_usercfg_set_username(lastfm_usercfg *cfg, const char *username)
{
        g_return_if_fail(cfg != NULL);
        g_free(cfg->username);
        cfg->username = g_strdup(username);
}

void
lastfm_usercfg_set_password(lastfm_usercfg *cfg, const char *password)
{
        g_return_if_fail(cfg != NULL);
        g_free(cfg->password);
        cfg->password = g_strdup(password);
}

lastfm_usercfg *
lastfm_usercfg_new(void)
{
        lastfm_usercfg *cfg = g_new0(lastfm_usercfg, 1);
        cfg->username = g_strdup("");
        cfg->password = g_strdup("");
        return cfg;
}

void
lastfm_usercfg_destroy(lastfm_usercfg *cfg)
{
        g_free(cfg->username);
        g_free(cfg->password);
        g_free(cfg);
}

lastfm_usercfg *
read_usercfg(void)
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
                return FALSE;
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
                        gsize len;
                        char *pw = (char *) g_base64_decode(val, &len);
                        pw = g_realloc(pw, len+1);
                        pw[len] = '\0';
                        lastfm_usercfg_set_password(cfg, pw);
                        g_free(pw);
                        g_free(val);
                } else if ((val = cfg_get_val(buf, "discovery")) != NULL) {
                        if (!strcmp(val, "1")) {
                                cfg->discovery_mode = TRUE;
                        } else {
                                cfg->discovery_mode = FALSE;
                        }
                }
        }
        fclose(fd);
        return cfg;
}

gboolean
write_usercfg(lastfm_usercfg *cfg)
{
        g_return_val_if_fail(cfg && cfg->username && cfg->password, FALSE);
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
        base64pw = g_base64_encode((guchar *)cfg->password,
                                   strlen(cfg->password));
        if (fprintf(fd, "username=\"%s\"\npassword=\"%s\"\n",
                    cfg->username, base64pw) <= 0) {
                g_warning("Error writing to config file");
                retval = FALSE;
        }
        g_free(base64pw);
        fclose(fd);
        return retval;
}
