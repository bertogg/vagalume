
#ifndef HTTP_H
#define HTTP_H

#include <glib.h>

char *escape_url(const char *url, gboolean escape);
void http_get_buffer(const char *url, char **buffer, size_t *bufsize);
void http_init(void);

#endif
