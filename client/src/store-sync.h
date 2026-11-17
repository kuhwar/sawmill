#pragma once

#include <gio/gio.h>

G_BEGIN_DECLS

/* Keeps a shallow, sparse git clone of the catalog repository (only catalog/ checked out).
 * The first run clones; later runs fetch and hard-reset, so local edits never block an update. */
void      store_sync_async   (const char           *repo_url,
                              const char           *clone_dir,
                              GCancellable         *cancellable,
                              GAsyncReadyCallback   callback,
                              gpointer              user_data);

/* Returns TRUE on success; *changed tells whether the catalog differs from before. */
gboolean  store_sync_finish  (GAsyncResult         *result,
                              gboolean             *changed,
                              GError              **error);

G_END_DECLS
