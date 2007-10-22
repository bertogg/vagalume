
#include <glib.h>

#include "radio.h"

char *
lastfm_globaltag_radio_url(const char* tag)
{
        g_return_val_if_fail(tag != NULL, NULL);
        return g_strconcat("lastfm://globaltags/", tag, NULL);
}

char *
lastfm_group_radio_url(const char* group)
{
        g_return_val_if_fail(group != NULL, NULL);
        return g_strconcat("lastfm://group/", group, NULL);
}

char *
lastfm_lovedtracks_radio_url(const char* user)
{
        g_return_val_if_fail(user != NULL, NULL);
        return g_strconcat("lastfm://user/", user, "/loved", NULL);
}

char *
lastfm_neighbours_radio_url(const char* user)
{
        g_return_val_if_fail(user != NULL, NULL);
        return g_strconcat("lastfm://user/", user, "/neighbours", NULL);
}

char *
lastfm_personal_radio_url(const char* user)
{
        g_return_val_if_fail(user != NULL, NULL);
        return g_strconcat("lastfm://user/", user, "/personal", NULL);
}

char *
lastfm_recommended_radio_url(const char *user, int val)
{
        g_return_val_if_fail(user != NULL && val >= 0 && val <= 100, NULL);
        char value[4];
        g_snprintf(value, 4, "%d", val);
        return g_strconcat("lastfm://user/", user,
                           "/recommended/", value, NULL);
}

char *
lastfm_similar_artist_radio_url(const char* artist)
{
        g_return_val_if_fail(artist != NULL, NULL);
        return g_strconcat("lastfm://artist/", artist, NULL);
}

char *
lastfm_usertag_radio_url(const char *user, const char *tag)
{
        g_return_val_if_fail(user != NULL && tag != NULL, NULL);
        return g_strconcat("lastfm://usertags/", user, "/", tag, NULL);
}
