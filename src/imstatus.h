/*
 * imstatus.h -- set IM status to current track
 * Copyright (C) 2008 Tim Wegener <twegener@fastmail.fm>
 *
 * This file is part of Vagalume and is published under the GNU GPLv3
 * See the README file for more details.
 */

#ifndef IMSTATUS_H
#define IMSTATUS_H

#include "config.h"
#include "playlist.h"
#include "userconfig.h"

#ifdef SET_IM_STATUS

void im_set_status(const lastfm_usercfg *cfg, const lastfm_track *track);
void im_clear_status(const lastfm_usercfg *cfg);

#else

void im_set_status(const lastfm_usercfg *cfg, const lastfm_track *track) { }
void im_clear_status(const lastfm_usercfg *cfg) { }

#endif /* SET_IM_STATUS */

#endif
