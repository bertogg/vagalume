/*
 * imstatus.h -- set IM status to current track
 * Copyright (C) 2008 Tim Wegener <twegener@fastmail.fm>
 *
 * This file is part of Vagalume and is published under the GNU GPLv3
 * See the README file for more details.
 */

#ifndef IMSTATUS_H
#define IMSTATUS_H

#include <glib.h>
#include "config.h"
#include "playlist.h"

#ifdef SET_IM_STATUS

void im_set_cfg(gboolean pidgin, gboolean gajim, gboolean gossip,
                gboolean telepathy);
void im_set_status(const lastfm_track *track);
void im_clear_status(void);

#else

void im_set_cfg(gboolean pidgin, gboolean gajim, gboolean gossip,
                gboolean telepathy) { }
void im_set_status(const lastfm_track *track) { }
void im_clear_status(void) { }

#endif /* SET_IM_STATUS */

#endif
