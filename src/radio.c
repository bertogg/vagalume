
#include <glib.h>

#include "radio.h"

char *
lastfm_radio_url(lastfm_radio type, const char *data)
{
        g_return_val_if_fail(data != NULL, NULL);
        switch (type) {
        case LASTFM_GLOBALTAG_RADIO:
                return g_strconcat("lastfm://globaltags/", data, NULL);
        case LASTFM_GROUP_RADIO:
                return g_strconcat("lastfm://group/", data, NULL);
        case LASTFM_LOVEDTRACKS_RADIO:
                return g_strconcat("lastfm://user/", data, "/loved", NULL);
        case LASTFM_NEIGHBOURS_RADIO:
                return g_strconcat("lastfm://user/", data,
                                   "/neighbours", NULL);
        case LASTFM_PERSONAL_RADIO:
                return g_strconcat("lastfm://user/", data,
                                   "/personal", NULL);
        case LASTFM_SIMILAR_ARTIST_RADIO:
                return g_strconcat("lastfm://artist/", data, NULL);
        case LASTFM_USERPLAYLIST_RADIO:
                return g_strconcat("lastfm://user/", data, "/playlist", NULL);
        case LASTFM_RECOMMENDED_RADIO:
        case LASTFM_USERTAG_RADIO:
                g_critical("Radio type %d is not handled here", type);
                return NULL;
        default:
                g_critical("Unknown radio type %d", type);
                return NULL;
        }
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
lastfm_usertag_radio_url(const char *user, const char *tag)
{
        g_return_val_if_fail(user != NULL && tag != NULL, NULL);
        return g_strconcat("lastfm://usertags/", user, "/", tag, NULL);
}
