#pragma once

#include <gio/gio.h>

G_BEGIN_DECLS

/* When Sawmill runs as an AppImage from anywhere else (e.g. ~/Downloads), move it to
 *   ~/.local/share/sawmill/<SAWMILL_SELF_ID>/app.AppImage
 * and add its launcher, exactly like an app installed from the catalog. When it already runs from
 * there, only make sure the launcher and icon exist. Does nothing for builds that are not AppImages,
 * or when SAWMILL_NO_SELF_INSTALL=1. */
void      store_self_install_async  (GAsyncReadyCallback  callback,
                                     gpointer             user_data);

/* Returns a message to show the user when Sawmill was moved, NULL otherwise. */
char     *store_self_install_finish (GAsyncResult        *result,
                                     GError             **error);

G_END_DECLS
