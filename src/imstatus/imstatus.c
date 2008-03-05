/*
 * imstatus.c -- set IM status to current track
 * Copyright (C) 2008 Tim Wegener <twegener@fastmail.fm>
 *
 * This file is part of Vagalume and is published under the GNU GPLv3
 * See the README file for more details.
 *
 * todo: perhaps this should be incorporated into controller.c instead
 */

#include <glib/gprintf.h>
#include "imstatus/imstatus.h"
#include "imstatus/impidgin.h"
#include "imstatus/imgajim.h"
#include "imstatus/imgossip.h"
#include "imstatus/imtelepathy.h"

static gboolean do_pidgin = TRUE;
static gboolean do_gajim = TRUE;
static gboolean do_gossip = TRUE;
static gboolean do_telepathy = TRUE;

void im_set_cfg(gboolean pidgin, gboolean gajim, gboolean gossip,
		gboolean telepathy)
{
        do_pidgin = pidgin;
        do_gajim = gajim;
        do_gossip = gossip;
        do_telepathy = telepathy;
}

void
im_set_status(const lastfm_track *track)
{
        /* todo: support customizable message format */
        char *message;
        message = g_strdup_printf("♫ %s - %s ♫", track->artist, track->title);
        if (do_pidgin) pidgin_set_status(message);
        if (do_gajim) gajim_set_status(message);
        if (do_gossip) gossip_set_status(message);
        if (do_telepathy) telepathy_set_status(message);
        g_free(message);
}

void
im_clear_status(void)
{
        if (do_pidgin) pidgin_set_status("");
        if (do_gajim) gajim_set_status("");
        if (do_gossip) gossip_set_status("");
        if (do_telepathy) telepathy_set_status("");
}
