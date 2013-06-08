/*
 * vgl-object.h -- Very minimalist implementation of ref-counted objects
 *
 * Copyright (C) 2009, 2010 Igalia, S.L.
 * Authors: Alberto Garcia <berto@igalia.com>
 *
 * This file is part of Vagalume and is published under the GNU GPLv3
 * See the README file for more details.
 */

#ifndef VGL_OBJECT_H
#define VGL_OBJECT_H

#include <glib.h>

G_BEGIN_DECLS

typedef struct {
#ifndef G_DISABLE_CAST_CHECKS
        int header;
#endif
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
