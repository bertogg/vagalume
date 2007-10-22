
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <regex.h>

#include "userconfig.h"

#define CONFIG_FILE ".lastfmrc"

static char *username = NULL;
static char *password = NULL;

const char *
user_cfg_get_usename(void)
{
        return username;
}

const char *
user_cfg_get_password(void)
{
        return password;
}

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

gboolean
read_user_cfg(void)
{
        if (username != NULL || password != NULL) return TRUE;
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
        while (fgets(buf, bufsize, fd)) {
                int len = strlen(buf);
                if (len == 0) continue;
                if (buf[len-1] == '\n') buf[len-1] = '\0';
                char *val;
                if ((val = cfg_get_val(buf, "username")) != NULL) {
                        g_free(username);
                        username = val;
                } else if ((val = cfg_get_val(buf, "password")) != NULL) {
                        g_free(password);
                        password = val;
                }
        }
        fclose(fd);
        return TRUE;
}

gboolean
write_user_cfg(void)
{
        g_return_val_if_fail(username != NULL && password != NULL, FALSE);
        gboolean retval = TRUE;
        char *cfgfile;
        FILE *fd = NULL;
        cfgfile = get_cfg_filename();
        if (cfgfile != NULL) fd = fopen(cfgfile, "w");
        g_free(cfgfile);
        if (fd == NULL) {
                g_warning("Unable to write config file");
                return FALSE;
        }
        if (fprintf(fd, "username=\"%s\"\npassword=\"%s\"\n",
                    username, password) <= 0) {
                g_warning("Error writing to config file");
                retval = FALSE;
        }
        fclose(fd);
        return retval;
}
