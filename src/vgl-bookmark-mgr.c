/*
 * vgl-bookmark-mgr.c -- Bookmark manager class
 *
 * Copyright (C) 2007-2008 Igalia, S.L.
 * Authors: Alberto Garcia <agarcia@igalia.com>
 *
 * This file is part of Vagalume and is published under the GNU GPLv3
 * See the README file for more details.
 */

#include "globaldefs.h"
#include "vgl-bookmark-mgr.h"
#include "userconfig.h"
#include "util.h"

#include <libxml/parser.h>
#include <glib.h>

enum {
  ADDED,
  REMOVED,
  CHANGED,
  N_SIGNALS
};

static guint mgr_signals[N_SIGNALS] = { 0 };

G_DEFINE_TYPE (VglBookmarkMgr, vgl_bookmark_mgr, G_TYPE_OBJECT);

#define VGL_BOOKMARK_MGR_GET_PRIVATE(object)                           \
        (G_TYPE_INSTANCE_GET_PRIVATE ((object), VGL_TYPE_BOOKMARK_MGR, \
                                      VglBookmarkMgrPrivate))

struct _VglBookmarkMgrPrivate {
        GList *bookmarks;
        int min_unused_id;
        gboolean dirty;
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

static void
vgl_bookmark_change(VglBookmark *bookmark, const char *newname,
                    const char *newurl)
{
        g_return_if_fail(bookmark != NULL);
        if (newname != NULL && newname != bookmark->name) {
                g_free((char *)bookmark->name);
                bookmark->name = g_strdup(newname);
        }
        if (newurl != NULL && newurl != bookmark->url) {
                g_free((char *)bookmark->url);
                bookmark->url = g_strdup(newurl);
        }
}

static int
vgl_bookmark_compare (gconstpointer bookmark, gconstpointer bookmark_id)
{
        const VglBookmark *bmk = (const VglBookmark *) bookmark;
        int id = GPOINTER_TO_INT (bookmark_id);
        g_return_val_if_fail (bmk != NULL && id >= 0, 0);
        if (bmk->id == id) {
                return 0;
        } else {
                return (bmk->id > id) ? 1 : -1;
        }
}

static const char *
vgl_bookmark_mgr_get_cfgfile(void)
{
        static char *cfgfilename = NULL;

        if (cfgfilename == NULL) {
                const char *cfgdir = vgl_user_cfg_get_cfgdir ();
                if (cfgdir != NULL) {
                        cfgfilename = g_strconcat (cfgdir, "/bookmarks.xml",
                                                   NULL);
                }
        }

        return cfgfilename;
}

static void
vgl_bookmark_mgr_load_from_disk(VglBookmarkMgr *mgr)
{
        const char *cfgfile = vgl_bookmark_mgr_get_cfgfile ();
        xmlDoc *doc = NULL;
        xmlNode *node = NULL;

        g_return_if_fail (VGL_IS_BOOKMARK_MGR (mgr) && cfgfile != NULL);

        /* Load bookmark file */
        if (file_exists (cfgfile)) {
                doc = xmlParseFile (cfgfile);
                if (doc == NULL) {
                        g_warning ("Bookmark file is not an XML document");
                }
        }

        /* Get root element */
        if (doc != NULL) {
                xmlNode *root = xmlDocGetRootElement (doc);
                xmlChar *mv = xmlGetProp (root, (xmlChar *) "version");
                if (mv == NULL ||
                    !xmlStrEqual (root->name, (xmlChar *) "bookmarks")) {
                        g_warning ("Error parsing bookmark file");
                } else if (!xmlStrEqual (mv, (xmlChar *) "1")) {
                        g_warning ("This bookmark file is version %s, but "
                                   "Vagalume " APP_VERSION " can only "
                                   "read version 1", mv);
                } else {
                        node = root->xmlChildrenNode;
                }
                if (mv != NULL) xmlFree (mv);
        }

        /* Parse each bookmark */
        for (; node != NULL; node = node->next) {
                const xmlChar *nodename = node->name;
                if (xmlStrEqual (nodename, (xmlChar *) "bookmark")) {
                        char *name, *url;
                        const xmlNode *subnode = node->xmlChildrenNode;
                        xml_get_string (doc, subnode, "name", &name);
                        xml_get_string (doc, subnode, "url", &url);
                        if (name && *name != '\0' && url && *url != '\0') {
                                vgl_bookmark_mgr_add_bookmark (mgr, name, url);
                        }
                        g_free (name);
                        g_free (url);
                }
        }

        if (doc != NULL) xmlFreeDoc(doc);

        mgr->priv->dirty = FALSE;
}

void
vgl_bookmark_mgr_save_to_disk (VglBookmarkMgr *mgr, gboolean force)
{
        const GList *l;
        const char *cfgfile = vgl_bookmark_mgr_get_cfgfile ();
        xmlDoc *doc;
        xmlNode *root;

        g_return_if_fail (VGL_IS_BOOKMARK_MGR (mgr) && cfgfile != NULL);

        if (!(mgr->priv->dirty) && !force)
                return;

        doc = xmlNewDoc ((xmlChar *) "1.0");
        root = xmlNewNode (NULL, (xmlChar *) "bookmarks");
        xmlSetProp (root, (xmlChar *) "version", (xmlChar *) "1");
        xmlSetProp (root, (xmlChar *) "revision", (xmlChar *) "1");
        xmlDocSetRootElement (doc, root);

        for (l = mgr->priv->bookmarks; l != NULL; l = l->next) {
                xmlNode *bmknode;
                const VglBookmark *bmk = (const VglBookmark *) l->data;
                bmknode = xmlNewNode (NULL, (xmlChar *) "bookmark");
                xmlAddChild (root, bmknode);
                xml_add_string (bmknode, "name", bmk->name);
                xml_add_string (bmknode, "url", bmk->url);
        }

        if (xmlSaveFormatFileEnc (cfgfile, doc, "UTF-8", 1) != -1) {
                mgr->priv->dirty = FALSE;
        } else {
                g_critical ("Unable to open %s", cfgfile);
        }

        xmlFreeDoc (doc);
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
        self->priv = priv;
        priv->bookmarks = NULL;
        priv->min_unused_id = 0;
        priv->dirty = FALSE;
        vgl_bookmark_mgr_load_from_disk (self);
}

static void
vgl_bookmark_mgr_finalize(GObject *object)
{
        VglBookmarkMgrPrivate *priv = VGL_BOOKMARK_MGR (object)->priv;

        vgl_bookmark_mgr_save_to_disk (VGL_BOOKMARK_MGR (object), FALSE);

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
        mgr_signals[ADDED] =
                g_signal_new ("bookmark-added",
                              G_OBJECT_CLASS_TYPE (klass),
                              G_SIGNAL_RUN_FIRST,
                              0, NULL, NULL,
                              g_cclosure_marshal_VOID__POINTER,
                              G_TYPE_NONE, 1, G_TYPE_POINTER);
        mgr_signals[REMOVED] =
                g_signal_new ("bookmark-removed",
                              G_OBJECT_CLASS_TYPE (klass),
                              G_SIGNAL_RUN_FIRST,
                              0, NULL, NULL,
                              g_cclosure_marshal_VOID__INT,
                              G_TYPE_NONE, 1, G_TYPE_INT);
        mgr_signals[CHANGED] =
                g_signal_new ("bookmark-changed",
                              G_OBJECT_CLASS_TYPE (klass),
                              G_SIGNAL_RUN_FIRST,
                              0, NULL, NULL,
                              g_cclosure_marshal_VOID__POINTER,
                              G_TYPE_NONE, 1, G_TYPE_POINTER);
}

VglBookmarkMgr *
vgl_bookmark_mgr_get_instance(void)
{
        /* Singleton, only one bookmark manager can exist */
        static VglBookmarkMgr *mgr = NULL;
        if (mgr == NULL) {
                mgr = vgl_bookmark_mgr_new();
                g_object_add_weak_pointer (G_OBJECT (mgr), (gpointer) &mgr);
        }
        return mgr;
}

static GList *
vgl_bookmark_mgr_find_bookmark_node(VglBookmarkMgr *mgr, int id)
{
        g_return_val_if_fail(VGL_IS_BOOKMARK_MGR(mgr) && id >= 0, NULL);
        return g_list_find_custom (mgr->priv->bookmarks, GINT_TO_POINTER (id),
                                   vgl_bookmark_compare);
}

int
vgl_bookmark_mgr_add_bookmark(VglBookmarkMgr *mgr,
                              const char *name, const char *url)
{
        g_return_val_if_fail(VGL_IS_BOOKMARK_MGR(mgr) && name && url, -1);
        VglBookmark *bmk;
        VglBookmarkMgrPrivate *priv = mgr->priv;

        bmk = vgl_bookmark_new(priv->min_unused_id, name, url);
        priv->bookmarks = g_list_append(priv->bookmarks, bmk);
        priv->min_unused_id++;
        priv->dirty = TRUE;
        g_signal_emit(mgr, mgr_signals[ADDED], 0, bmk);
        return bmk->id;
}

void
vgl_bookmark_mgr_remove_bookmark(VglBookmarkMgr *mgr, int id)
{
        g_return_if_fail(VGL_IS_BOOKMARK_MGR(mgr) && id >= 0);
        VglBookmarkMgrPrivate *priv = mgr->priv;
        GList *l = vgl_bookmark_mgr_find_bookmark_node(mgr, id);
        if (l != NULL) {
                VglBookmark *bmk = (VglBookmark *) l->data;
                vgl_bookmark_destroy(bmk);
                priv->bookmarks = g_list_delete_link(
                        priv->bookmarks, l);
                priv->dirty = TRUE;
                g_signal_emit(mgr, mgr_signals[REMOVED], 0, id);
        } else {
                g_warning("%s: bookmark %d does not exist", __FUNCTION__, id);
        }
}

void
vgl_bookmark_mgr_change_bookmark(VglBookmarkMgr *mgr,
                                 int id, char *newname, char *newurl)
{
        g_return_if_fail(VGL_IS_BOOKMARK_MGR(mgr) && id >= 0);
        GList *l = vgl_bookmark_mgr_find_bookmark_node(mgr, id);
        if (l != NULL) {
                VglBookmark *bmk = (VglBookmark *) l->data;
                vgl_bookmark_change(bmk, newname, newurl);
                mgr->priv->dirty = TRUE;
                g_signal_emit(mgr, mgr_signals[CHANGED], 0, bmk);
        } else {
                g_warning("%s: bookmark %d does not exist", __FUNCTION__, id);
        }
}

const VglBookmark *
vgl_bookmark_mgr_get_bookmark(VglBookmarkMgr *mgr, int id)
{
        g_return_val_if_fail(VGL_IS_BOOKMARK_MGR(mgr) && id >= 0, NULL);
        GList *l = vgl_bookmark_mgr_find_bookmark_node(mgr, id);
        if (l != NULL) {
                return (const VglBookmark *) l->data;
        }
        return NULL;
}

const GList *
vgl_bookmark_mgr_get_bookmark_list(VglBookmarkMgr *mgr)
{
        g_return_val_if_fail(VGL_IS_BOOKMARK_MGR(mgr), NULL);
        return mgr->priv->bookmarks;
}

/**
 * Reorders the list of bookmarks using the bookmark IDs as keys.
 *
 * @param mgr The bookmark manager
 * @para ids An array indicating the new order, finishing with -1
 */
void
vgl_bookmark_mgr_reorder (VglBookmarkMgr *mgr, const int *ids)
{
        g_return_if_fail (VGL_IS_BOOKMARK_MGR (mgr));
        VglBookmarkMgrPrivate *priv = mgr->priv;
        GList *iter;
        int pos = 0;
        for (iter = priv->bookmarks; iter != NULL; iter = iter->next, pos++) {
                VglBookmark *bmk = (VglBookmark *) iter->data;
                int id = ids [pos];
                g_return_if_fail (bmk != NULL && id != -1);
                if (bmk->id != id) {
                        GList *l;
                        l = g_list_find_custom (iter, GINT_TO_POINTER (id),
                                                vgl_bookmark_compare);
                        g_return_if_fail (l != NULL);
                        iter->data = l->data;
                        l->data = bmk;
                }
        }
        priv->dirty = TRUE;
        g_return_if_fail (ids [pos] == -1);
}
