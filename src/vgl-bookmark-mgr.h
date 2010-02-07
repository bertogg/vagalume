/*
 * vgl-bookmark-mgr.h -- Bookmark manager class
 *
 * Copyright (C) 2007-2008, 2010 Igalia, S.L.
 * Authors: Alberto Garcia <agarcia@igalia.com>
 *
 * This file is part of Vagalume and is published under the GNU GPLv3
 * See the README file for more details.
 */

#ifndef VGL_BOOKMARK_MGR_H
#define VGL_BOOKMARK_MGR_H

#include "vgl-bookmark-mgr.bp.h"

G_BEGIN_DECLS

typedef struct {
        int id;
        const char *name;
        const char *url;
} VglBookmark;

struct _VglBookmarkMgrClass
{
        GObjectClass parent_class;
};

struct _VglBookmarkMgr
{
        GObject parent;
        VglBookmarkMgrPrivate *priv;
};

VglBookmarkMgr *
vgl_bookmark_mgr_get_instance           (void);

int
vgl_bookmark_mgr_add_bookmark           (VglBookmarkMgr *mgr,
                                         const char     *name,
                                         const char     *url);

void
vgl_bookmark_mgr_remove_bookmark        (VglBookmarkMgr *mgr,
                                         int             id);

void
vgl_bookmark_mgr_change_bookmark        (VglBookmarkMgr *mgr,
                                         int             id,
                                         char           *newname,
                                         char           *newurl);

const VglBookmark *
vgl_bookmark_mgr_get_bookmark           (VglBookmarkMgr *mgr,
                                         int             id);

const GList *
vgl_bookmark_mgr_get_bookmark_list      (VglBookmarkMgr *mgr);

void
vgl_bookmark_mgr_save_to_disk           (VglBookmarkMgr *mgr,
                                         gboolean        force);

void
vgl_bookmark_mgr_reorder                (VglBookmarkMgr *mgr,
                                         const int      *ids);

G_END_DECLS

#endif                          /* VGL_BOOKMARK_MGR_H */
