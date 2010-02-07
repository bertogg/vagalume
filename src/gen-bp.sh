#!/bin/sh

# Script to generate GObject boilerplate
# Copyright (C) 2010 Igalia, S.L.
# Author: Alberto Garcia <agarcia@igalia.com>

if [ "$#" -ne "1" ]; then
    echo "Usage: gen-bp.sh some-object-name.bp.h"
    exit 1
fi

stripped=`echo $1 | cut -d '.' -f 1` # 'some-object-name'

prefix_lc=`echo $stripped | cut -d '-' -f 1` # 'some'
prefix_uc=`echo $prefix_lc | tr a-z A-Z`     # 'SOME'

suffix_lc=`echo $stripped | cut -d '-' -f 2- | tr '-' '_'` # 'object_name'
suffix_uc=`echo $suffix_lc | tr a-z A-Z`                   # 'OBJECT_NAME'

full_name_lc="${prefix_lc}_${suffix_lc}"     # 'some_object_name'
full_name_uc="${prefix_uc}_${suffix_uc}"     # 'SOME_OBJECT_NAME'

type_name="${prefix_uc}_TYPE_${suffix_uc}"   # 'SOME_TYPE_OBJECT_NAME'

object_name=""                               # 'SomeObjectName'

for part in `echo $stripped | tr '-' ' '`; do
    first=`echo $part | cut -c 1 | tr a-z A-Z`
    rest=`echo $part | cut -c 2-`
    object_name="${object_name}${first}${rest}"
done

class_name="${object_name}Class"             # 'SomeObjectNameClass'
private_name="${object_name}Private"         # 'SomeObjectNamePrivate'

cat <<EOF

#ifndef __${full_name_uc}_BP__
#define __${full_name_uc}_BP__

#include <glib-object.h>

G_BEGIN_DECLS

#define ${type_name} (${full_name_lc}_get_type())
#define ${full_name_uc}(obj)                                          \\
   (G_TYPE_CHECK_INSTANCE_CAST ((obj),                                \\
   ${type_name}, ${object_name}))
#define ${full_name_uc}_CLASS(klass)                                  \\
   (G_TYPE_CHECK_CLASS_CAST ((klass),                                 \\
   ${type_name}, ${class_name}))
#define ${prefix_uc}_IS_${suffix_uc}(obj)                             \\
   (G_TYPE_CHECK_INSTANCE_TYPE ((obj), ${type_name}))
#define ${prefix_uc}_IS_${suffix_uc}_CLASS(klass)                     \\
   (G_TYPE_CHECK_CLASS_TYPE ((klass), ${type_name}))
#define ${full_name_uc}_GET_CLASS(obj)                                \\
   (G_TYPE_INSTANCE_GET_CLASS ((obj),                                 \\
   ${type_name}, ${class_name}))

typedef struct _${object_name}          ${object_name};
typedef struct _${class_name}           ${class_name};
typedef struct _${private_name}         ${private_name};

GType
${full_name_lc}_get_type                (void) G_GNUC_CONST;

G_END_DECLS

#endif /* __${full_name_uc}_BP__ */

EOF
