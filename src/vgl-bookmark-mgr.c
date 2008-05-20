/*
 * vgl-bookmark-mgr.c -- Bookmark manager class
 * Copyright (C) 2008 Alberto Garcia <agarcia@igalia.com>
 *
 * This file is part of Vagalume and is published under the GNU GPLv3
 * See the README file for more details.
 */

#include "vgl-bookmark-mgr.h"

#include <glib.h>

/* Singleton, only one bookmark manager can exist */
static VglBookmarkMgr *bookmark_mgr = NULL;

G_DEFINE_TYPE (VglBookmarkMgr, vgl_bookmark_mgr, G_TYPE_OBJECT);

#define VGL_BOOKMARK_MGR_GET_PRIVATE(object)                           \
        (G_TYPE_INSTANCE_GET_PRIVATE ((object), VGL_TYPE_BOOKMARK_MGR, \
                                      VglBookmarkMgrPrivate))

typedef struct _VglBookmarkMgrPrivate VglBookmarkMgrPrivate;
struct _VglBookmarkMgrPrivate {
        GList *bookmarks;
        int min_unused_id;
};

static VglBookmark *
vgl_bookmark_new(int id, const char *name, const char *url)
{
        g_return_val_if_fail(name != NULL && url != NULL, NULL);
        VglBookmark *bookmark;
        bookmark = g_slice_new(VglBookmark);
        bookmark->id = id;
        bookmark->name = g_strdup(name);
        bookmark->url = g_strdup(url);
        return bookmark;
}

static void
vgl_bookmark_destroy(VglBookmark *bookmark)
{
        g_return_if_fail(bookmark != NULL);
        g_free((char *)bookmark->name);
        g_free((char *)bookmark->url);
        g_slice_free(VglBookmark, bookmark);
}

static VglBookmarkMgr *
vgl_bookmark_mgr_new(void)
{
        return g_object_new(VGL_TYPE_BOOKMARK_MGR, NULL);
}

static void
vgl_bookmark_mgr_init(VglBookmarkMgr *self)
{
        VglBookmarkMgrPrivate *priv = VGL_BOOKMARK_MGR_GET_PRIVATE(self);
        priv->bookmarks = NULL;
        priv->min_unused_id = 0;
}

static void
vgl_bookmark_mgr_finalize(GObject *object)
{
        VglBookmarkMgrPrivate *priv = VGL_BOOKMARK_MGR_GET_PRIVATE(object);

        g_list_foreach(priv->bookmarks, (GFunc) vgl_bookmark_destroy, NULL);
        g_list_free(priv->bookmarks);

        g_signal_handlers_destroy(object);
        G_OBJECT_CLASS(vgl_bookmark_mgr_parent_class)->finalize(object);
}

static void
vgl_bookmark_mgr_class_init(VglBookmarkMgrClass *klass)
{
        GObjectClass *gobject_class = (GObjectClass *)klass;
        gobject_class->finalize = vgl_bookmark_mgr_finalize;
        g_type_class_add_private(klass, sizeof(VglBookmarkMgrPrivate));
}

VglBookmarkMgr *
vgl_bookmark_mgr_get_instance(void)
{
        if (bookmark_mgr == NULL) {
                bookmark_mgr = vgl_bookmark_mgr_new();
        }
        return bookmark_mgr;
}

int
vgl_bookmark_mgr_add_bookmark(VglBookmarkMgr *mgr,
                                  const char *name, const char *url)
{
        g_return_val_if_fail(VGL_IS_BOOKMARK_MGR(mgr) && name && url, -1);
        VglBookmark *bmk;
        VglBookmarkMgrPrivate *priv = VGL_BOOKMARK_MGR_GET_PRIVATE(mgr);

        bmk = vgl_bookmark_new(priv->min_unused_id, name, url);
        priv->bookmarks = g_list_prepend(priv->bookmarks, bmk);
        priv->min_unused_id++;

        return bmk->id;
}

void
vgl_bookmark_mgr_remove_bookmark(VglBookmarkMgr *mgr, int id)
{
        g_return_if_fail(VGL_IS_BOOKMARK_MGR(mgr) && id >= 0);
        VglBookmarkMgrPrivate *priv = VGL_BOOKMARK_MGR_GET_PRIVATE(mgr);
        VglBookmark *bmk;
        GList *l;

        for (l = priv->bookmarks; l != NULL; l = l->next) {
                bmk = (VglBookmark *) l->data;
                if (bmk->id == id) {
                        vgl_bookmark_destroy(bmk);
                        priv->bookmarks = g_list_delete_link(
                                priv->bookmarks, l);
                        return;
                }
        }
        g_warning("%s: bookmark %d does not exist", __FUNCTION__, id);
}

const VglBookmark *
vgl_bookmark_mgr_get_bookmark(VglBookmarkMgr *mgr, int id)
{
        g_return_val_if_fail(VGL_IS_BOOKMARK_MGR(mgr) && id >= 0, NULL);
        VglBookmarkMgrPrivate *priv = VGL_BOOKMARK_MGR_GET_PRIVATE(mgr);
        VglBookmark *bmk;
        GList *l;

        for (l = priv->bookmarks; l != NULL; l = l->next) {
                bmk = (VglBookmark *) l->data;
                if (bmk->id == id) {
                        return bmk;
                }
        }
        return NULL;
}

const GList *
vgl_bookmark_mgr_get_bookmark_list(VglBookmarkMgr *mgr)
{
        g_return_val_if_fail(VGL_IS_BOOKMARK_MGR(mgr), NULL);
        VglBookmarkMgrPrivate *priv = VGL_BOOKMARK_MGR_GET_PRIVATE(mgr);
        return priv->bookmarks;
}
