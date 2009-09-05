/*
 * vgl-object.c -- Very minimalist implementation of ref-counted objects
 *
 * Copyright (C) 2009 Igalia, S.L.
 * Authors: Alberto Garcia <agarcia@igalia.com>
 *
 * This file is part of Vagalume and is published under the GNU GPLv3
 * See the README file for more details.
 */

#include "vgl-object.h"

static const int vgl_object_header = 0x12bc0d9a;

gpointer
vgl_object_new_with_size                (int            objsize,
                                         GDestroyNotify destroy_func)
{
        VglObject *obj    = g_slice_alloc0 (objsize);

        obj->header       = vgl_object_header;
        obj->refcount     = 1;
        obj->objsize      = objsize;
        obj->destroy_func = destroy_func;

        return obj;
}

gpointer
vgl_object_ref                          (gpointer ptr)
{
        VglObject *obj = ptr;
        g_return_val_if_fail (ptr != NULL, NULL);
        g_return_val_if_fail (obj->header == vgl_object_header, NULL);
        g_atomic_int_inc (&(obj->refcount));
        return obj;
}

void
vgl_object_unref                        (gpointer ptr)
{
        VglObject *obj = ptr;
        g_return_if_fail (ptr != NULL);
        g_return_if_fail (obj->header == vgl_object_header);
        if (g_atomic_int_dec_and_test (&(obj->refcount))) {
                if (obj->destroy_func) {
                        obj->destroy_func (ptr);
                }
                obj->header = 0;
                g_slice_free1 (obj->objsize, obj);
        }
}
