/*
 * vgl-bookmark-mgr.h -- Bookmark manager class
 * Copyright (C) 2008 Alberto Garcia <agarcia@igalia.com>
 *
 * This file is part of Vagalume and is published under the GNU GPLv3
 * See the README file for more details.
 */

#ifndef VGL_BOOKMARK_MGR_H
#define VGL_BOOKMARK_MGR_H

#include <glib-object.h>

G_BEGIN_DECLS

#define VGL_TYPE_BOOKMARK_MGR (vgl_bookmark_mgr_get_type())
#define VGL_BOOKMARK_MGR(obj)                                        \
   (G_TYPE_CHECK_INSTANCE_CAST ((obj),                               \
   VGL_TYPE_BOOKMARK_MGR, VglBookmarkMgr))
#define VGL_BOOKMARK_MGR_CLASS(klass)                                \
   (G_TYPE_CHECK_CLASS_CAST ((klass),                                \
   VGL_TYPE_BOOKMARK_MGR, VglBookmarkMgrClass))
#define VGL_IS_BOOKMARK_MGR(obj)                                     \
   (G_TYPE_CHECK_INSTANCE_TYPE ((obj), VGL_TYPE_BOOKMARK_MGR))
#define VGL_IS_BOOKMARK_MGR_CLASS(klass)                             \
   (G_TYPE_CHECK_CLASS_TYPE ((klass), VGL_TYPE_BOOKMARK_MGR))
#define VGL_BOOKMARK_MGR_GET_CLASS(obj)                              \
   (G_TYPE_INSTANCE_GET_CLASS ((obj),                                \
   VGL_TYPE_BOOKMARK_MGR, VglBookmarkMgrClass))

typedef struct _VglBookmarkMgr      VglBookmarkMgr;
typedef struct _VglBookmarkMgrClass VglBookmarkMgrClass;

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
};

GType vgl_bookmark_mgr_get_type (void) G_GNUC_CONST;

VglBookmarkMgr *vgl_bookmark_mgr_get_instance(void);
int vgl_bookmark_mgr_add_bookmark(VglBookmarkMgr *mgr,
                                  const char *name, const char *url);
void vgl_bookmark_mgr_remove_bookmark(VglBookmarkMgr *mgr, int id);
void vgl_bookmark_mgr_change_bookmark(VglBookmarkMgr *mgr, int id,
                                      char *newname, char *newurl);
const VglBookmark *vgl_bookmark_mgr_get_bookmark(VglBookmarkMgr *mgr, int id);
const GList *vgl_bookmark_mgr_get_bookmark_list(VglBookmarkMgr *mgr);
void vgl_bookmark_mgr_save_to_disk (VglBookmarkMgr *mgr, gboolean force);
void vgl_bookmark_mgr_reorder(VglBookmarkMgr *mgr, const int *ids);

G_END_DECLS

#endif /* VGL_BOOKMARK_MGR_H */
