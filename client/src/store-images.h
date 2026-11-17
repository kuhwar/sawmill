#pragma once

#include <gtk/gtk.h>

G_BEGIN_DECLS

/* A picture that downloads an https image (cached in ~/.cache/sawmill/images) and shows a spinner
 * until it arrives, or a short message if it cannot be loaded. */
GtkWidget *store_remote_picture_new (const char *url,
                                     int         height);

G_END_DECLS
