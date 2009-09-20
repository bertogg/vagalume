/*
 * vgl-object.h -- Very minimalist implementation of ref-counted objects
 *
 * Copyright (C) 2009 Igalia, S.L.
 * Authors: Alberto Garcia <agarcia@igalia.com>
 *
 * This file is part of Vagalume and is published under the GNU GPLv3
 * See the README file for more details.
 */

#ifndef VGL_OBJECT_H
#define VGL_OBJECT_H

#include <glib.h>
#include <glib-object.h>

G_BEGIN_DECLS

/*
 * This is a very minimalist object implementation based on GObject.
 *
 * If you only need to create a very simple ref-counted object (with
 * no signals, no properties, no virtual functions, etc.) you can use
 * these macros to save you from writing some boilerplate code (class
 * structure, _init, _class_init, _finalize).
 *
 * How to define a new object
 * ==========================
 *
 * Put this in your foo-object.h:
 * --------------
 * typedef struct {
 *         GObject parent;
 *         gint value;
 *         gchar *ptr;
 * } FooObject;
 *
 * VGL_DEFINE_TYPE_HEADER (FooObject, foo_object)
 * --------------
 *
 * And this in your foo-object.c:
 * --------------
 * VGL_DEFINE_TYPE_CODE (FooObject, foo_object, foo_object_destroy)
 *
 * static void
 * foo_object_destroy (FooObject *obj)
 * {
 *         g_free (obj->ptr);
 * }
 * --------------
 *
 * or, if you don't need a destroy function, just this line:
 * --------------
 * VGL_DEFINE_TYPE_CODE_SIMPLE (FooObject, foo_object)
 * --------------
 *
 * If your object is private to a single source file (that is, you
 * don't want/need a foo-object.h), you can use one of these macros:
 * --------------
 * VGL_DEFINE_TYPE (FooObject, foo_object, foo_object_destroy)
 * VGL_DEFINE_TYPE_SIMPLE (FooObject, foo_object)
 * --------------
 *
 * To create an instance of FooObject just call foo_object_empty_new()
 * and you'll be returned a new, empty FooObject.
 */

#define VGL_DEFINE_TYPE_HEADER_FULL(TN, t_n, VISIBILITY)              \
        typedef struct {                                              \
                GObjectClass parent_class;                            \
        } TN##Class;                                                  \
                                                                      \
        VISIBILITY TN * t_n##_empty_new (void);                       \
        VISIBILITY GType t_n##_get_type (void) G_GNUC_CONST;


#define VGL_DEFINE_TYPE_HEADER(TN, t_n)                               \
        VGL_DEFINE_TYPE_HEADER_FULL(TN, t_n, extern)


#define VGL_DEFINE_TYPE_CODE_BEGIN(TN, t_n)                           \
        G_DEFINE_TYPE (TN, t_n, G_TYPE_OBJECT)                        \
        static void t_n##_init (TN *object) { }                       \
                                                                      \
        TN *                                                          \
        t_n##_empty_new (void)                                        \
        {                                                             \
                return (TN *) g_object_new (t_n##_get_type(), NULL);  \
        }


#define VGL_DEFINE_TYPE_CODE_SIMPLE(TN, t_n)                          \
        VGL_DEFINE_TYPE_CODE_BEGIN (TN, t_n)                          \
        static void t_n##_class_init (TN ## Class *klass) { }


#define VGL_DEFINE_TYPE_CODE(TN, t_n, DESTROY_FUNC)                   \
        VGL_DEFINE_TYPE_CODE_BEGIN (TN, t_n)                          \
        static void DESTROY_FUNC (TN *);                              \
                                                                      \
        static void                                                   \
        _##t_n##_finalize (GObject *obj)                              \
        {                                                             \
                GObjectClass *parent;                                 \
                parent = (GObjectClass *) t_n##_parent_class;         \
                DESTROY_FUNC ((TN *) obj);                            \
                parent->finalize (obj);                               \
        }                                                             \
                                                                      \
        static void                                                   \
        t_n##_class_init (TN ## Class *klass)                         \
        {                                                             \
                GObjectClass *gobject_class = (GObjectClass *) klass; \
                gobject_class->finalize = _##t_n##_finalize;          \
        }


#define VGL_DEFINE_TYPE_SIMPLE(TN, t_n)                               \
        VGL_DEFINE_TYPE_HEADER_FULL(TN, t_n, static)                  \
        VGL_DEFINE_TYPE_CODE_SIMPLE(TN, t_n)


#define VGL_DEFINE_TYPE(TN, t_n, DESTROY_FUNC)                        \
        VGL_DEFINE_TYPE_HEADER_FULL(TN, t_n, static)                  \
        VGL_DEFINE_TYPE_CODE(TN, t_n, DESTROY_FUNC)


typedef struct {
        int header;
        volatile int refcount;
        int objsize;
        GDestroyNotify destroy_func;
} VglObject;

gpointer
vgl_object_new_with_size                (int            objsize,
                                         GDestroyNotify destroy_func);

gpointer
vgl_object_ref                          (gpointer ptr);

void
vgl_object_unref                        (gpointer ptr);


#define vgl_object_new(type,func) \
((type *) \
vgl_object_new_with_size                (sizeof (type), func))

G_END_DECLS

#endif /* VGL_OBJECT_H */
