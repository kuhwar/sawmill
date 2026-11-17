#pragma once

#include <gio/gio.h>
#include "store-catalog.h"

G_BEGIN_DECLS

/* Installed files, for an app with id "github.com/owner/repo":
 *   ~/.local/share/sawmill/github.com/owner/repo/app.AppImage
 *   ~/.local/share/sawmill/github.com/owner/repo/icon.png|svg      (from inside the AppImage)
 *   ~/.local/share/sawmill/github.com/owner/repo/version           (installed release tag)
 *   ~/.local/share/applications/sawmill-github.com-owner-repo.desktop
 */
/* The same paths from an app id ("github.com/owner/repo"), for when no catalog entry is at hand. */
char     *store_install_dir_for          (const char *id);
char     *store_install_desktop_path_for (const char *id);

char     *store_install_dir           (StoreApp *app);
char     *store_install_appimage_path (StoreApp *app);
char     *store_install_desktop_path  (StoreApp *app);

/* Read the installed state of an app from disk into app->state / app->installed_version. */
void      store_install_load_state    (StoreApp *app);

/* Called on the main thread. fraction is 0..1, or < 0 while unknown. */
typedef void (*StoreProgressFunc) (StoreApp *app, const char *phase, double fraction, gpointer user_data);

/* Look up the latest release, download its AppImage, verify it, and add a launcher. */
void      store_install_async         (StoreApp            *app,
                                       GCancellable        *cancellable,
                                       StoreProgressFunc    progress,
                                       gpointer             progress_data,
                                       GAsyncReadyCallback  callback,
                                       gpointer             user_data);
/* Returns the installed release tag. */
char     *store_install_finish        (GAsyncResult        *result,
                                       GError             **error);

/* Remove the AppImage, its icon and its launcher. */
gboolean  store_uninstall             (StoreApp            *app,
                                       GError             **error);

/* Start the installed app through its launcher. */
gboolean  store_launch                (StoreApp            *app,
                                       GError             **error);

G_END_DECLS
