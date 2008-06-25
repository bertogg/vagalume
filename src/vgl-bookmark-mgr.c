/*
 * vgl-bookmark-mgr.c -- Bookmark manager class
 * Copyright (C) 2008 Alberto Garcia <agarcia@igalia.com>
 *
 * This file is part of Vagalume and is published under the GNU GPLv3
 * See the README file for more details.
 */

#include "vgl-bookmark-mgr.h"
#include "userconfig.h"
#include "util.h"

#include <libxml/parser.h>
#include <glib.h>

/* Singleton, only one bookmark manager can exist */
static VglBookmarkMgr *bookmark_mgr = NULL;

enum {
  ADDED,
  REMOVED,
  CHANGED,
  LAST_SIGNAL
};

static guint mgr_signals[LAST_SIGNAL] = { 0 };

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

static const char *
vgl_bookmark_mgr_get_cfgfile(void)
{
        static char *cfgfilename = NULL;

        if (cfgfilename == NULL) {
                const char *cfgdir = lastfm_usercfg_get_cfgdir ();
                if (cfgdir != NULL) {
                        cfgfilename = g_strconcat (cfgdir, "/bookmarks.xml",
                                                   NULL);
                }
        }

        return cfgfilename;
}

static void
vgl_bookmark_get_from_xml_node(xmlDoc *doc, xmlNode *node,
                               char **name, char **url)
{
        xmlNode *child;

        g_return_if_fail (node != NULL && name != NULL && url != NULL);

        for (child = node->xmlChildrenNode; child; child = child->next) {
                char *val = (char *) xmlNodeListGetString(
                        doc, child->xmlChildrenNode, 1);
                if (val == NULL) {
                        /* Ignore empty nodes */;
                } else if (!xmlStrcmp(child->name, (const xmlChar *) "name")) {
                        g_free (*name);
                        *name = g_strdup (val);
                } else if (!xmlStrcmp(child->name, (const xmlChar *) "url")) {
                        g_free (*url);
                        *url = g_strdup (val);
                }
                xmlFree ((xmlChar *) val);
        }
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
                if (!xmlStrcmp(root->name, (const xmlChar *) "bookmarks")) {
                        node = root->xmlChildrenNode;
                } else {
                        g_warning ("Error parsing bookmark file");
                }
        }

        /* Parse each bookmark */
        for (; node != NULL; node = node->next) {
                const xmlChar *nodename = node->name;
                if (!xmlStrcmp(nodename, (const xmlChar *) "bookmark")) {
                        char *name = NULL;
                        char *url = NULL;
                        vgl_bookmark_get_from_xml_node (doc,node,&name,&url);
                        if (name != NULL && url != NULL) {
                                vgl_bookmark_mgr_add_bookmark (mgr, name, url);
                        }
                        g_free (name);
                        g_free (url);
                }
        }

        if (doc != NULL) xmlFreeDoc(doc);
}

void
vgl_bookmark_mgr_save_to_disk(VglBookmarkMgr *mgr)
{
        const GList *l;
        const char *cfgfile = vgl_bookmark_mgr_get_cfgfile ();
        VglBookmarkMgrPrivate *priv;
        xmlDoc *doc;
        xmlNode *root;

        g_return_if_fail (VGL_IS_BOOKMARK_MGR (mgr) && cfgfile != NULL);
        priv = VGL_BOOKMARK_MGR_GET_PRIVATE (mgr);

        doc = xmlNewDoc ((xmlChar *) "1.0");;
        root = xmlNewNode (NULL, (xmlChar *) "bookmarks");
        xmlSetProp (root, (xmlChar *) "version", (xmlChar *) "1");
        xmlDocSetRootElement (doc, root);

        for (l = priv->bookmarks; l != NULL; l = l->next) {
                xmlNode *bmknode, *name, *url;
                xmlChar *enc;
                const VglBookmark *bmk = (const VglBookmark *) l->data;

                name = xmlNewNode (NULL, (xmlChar *) "name");
                enc = xmlEncodeEntitiesReentrant (NULL, (xmlChar *) bmk->name);
                xmlNodeSetContent (name, enc);
                xmlFree (enc);

                url = xmlNewNode (NULL, (xmlChar *) "url");
                enc = xmlEncodeEntitiesReentrant (NULL, (xmlChar *) bmk->url);
                xmlNodeSetContent (url, enc);
                xmlFree (enc);

                bmknode = xmlNewNode (NULL, (xmlChar *) "bookmark");
                xmlAddChild (root, bmknode);
                xmlAddChild (bmknode, name);
                xmlAddChild (bmknode, url);
        }

        if (xmlSaveFormatFileEnc (cfgfile, doc, "UTF-8", 1) == -1) {
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
        priv->bookmarks = NULL;
        priv->min_unused_id = 0;
        vgl_bookmark_mgr_load_from_disk (self);
}

static void
vgl_bookmark_mgr_finalize(GObject *object)
{
        VglBookmarkMgrPrivate *priv = VGL_BOOKMARK_MGR_GET_PRIVATE(object);

        vgl_bookmark_mgr_save_to_disk (VGL_BOOKMARK_MGR (object));

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
        if (bookmark_mgr == NULL) {
                bookmark_mgr = vgl_bookmark_mgr_new();
        }
        return bookmark_mgr;
}

static GList *
vgl_bookmark_mgr_find_bookmark_node(VglBookmarkMgr *mgr, int id)
{
        g_return_val_if_fail(VGL_IS_BOOKMARK_MGR(mgr) && id >= 0, NULL);
        VglBookmarkMgrPrivate *priv = VGL_BOOKMARK_MGR_GET_PRIVATE(mgr);
        GList *l;

        for (l = priv->bookmarks; l != NULL; l = l->next) {
                VglBookmark *bmk = (VglBookmark *) l->data;
                if (bmk->id == id) {
                        return l;
                }
        }
        return NULL;
}

int
vgl_bookmark_mgr_add_bookmark(VglBookmarkMgr *mgr,
                              const char *name, const char *url)
{
        g_return_val_if_fail(VGL_IS_BOOKMARK_MGR(mgr) && name && url, -1);
        VglBookmark *bmk;
        VglBookmarkMgrPrivate *priv = VGL_BOOKMARK_MGR_GET_PRIVATE(mgr);

        bmk = vgl_bookmark_new(priv->min_unused_id, name, url);
        priv->bookmarks = g_list_append(priv->bookmarks, bmk);
        priv->min_unused_id++;
        g_signal_emit(mgr, mgr_signals[ADDED], 0, bmk);
        return bmk->id;
}

void
vgl_bookmark_mgr_remove_bookmark(VglBookmarkMgr *mgr, int id)
{
        g_return_if_fail(VGL_IS_BOOKMARK_MGR(mgr) && id >= 0);
        VglBookmarkMgrPrivate *priv = VGL_BOOKMARK_MGR_GET_PRIVATE(mgr);
        GList *l = vgl_bookmark_mgr_find_bookmark_node(mgr, id);
        if (l != NULL) {
                VglBookmark *bmk = (VglBookmark *) l->data;
                vgl_bookmark_destroy(bmk);
                priv->bookmarks = g_list_delete_link(
                        priv->bookmarks, l);
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
        VglBookmarkMgrPrivate *priv = VGL_BOOKMARK_MGR_GET_PRIVATE(mgr);
        return priv->bookmarks;
}
