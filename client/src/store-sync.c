/* Catalog sync: a shallow, sparse git clone that only checks out catalog/. */

#include <errno.h>
#include <glib/gstdio.h>
#include "store-sync.h"

typedef struct {
  char *url;
  char *dir;
} SyncData;

static void
sync_data_free (SyncData *d)
{
  g_free (d->url);
  g_free (d->dir);
  g_free (d);
}

/* Run git with the given arguments; on failure, set error to git's own message. */
static gboolean
git (GCancellable *cancellable, char **out, GError **error, ...)
{
  g_autoptr (GPtrArray) argv = g_ptr_array_new ();
  g_autoptr (GSubprocessLauncher) launcher = g_subprocess_launcher_new (G_SUBPROCESS_FLAGS_STDOUT_PIPE |
                                                                        G_SUBPROCESS_FLAGS_STDERR_PIPE);
  g_autoptr (GSubprocess) proc = NULL;
  g_autofree char *stdout_buf = NULL, *stderr_buf = NULL;
  const char *arg;
  va_list ap;

  g_ptr_array_add (argv, (gpointer) "git");
  va_start (ap, error);
  while ((arg = va_arg (ap, const char *)))
    g_ptr_array_add (argv, (gpointer) arg);
  va_end (ap);
  g_ptr_array_add (argv, NULL);

  g_subprocess_launcher_setenv (launcher, "GIT_TERMINAL_PROMPT", "0", TRUE);   /* never ask for passwords */
  g_subprocess_launcher_setenv (launcher, "LC_ALL", "C", TRUE);
  proc = g_subprocess_launcher_spawnv (launcher, (const char * const *) argv->pdata, error);
  if (!proc)
    {
      if (error && g_error_matches (*error, G_SPAWN_ERROR, G_SPAWN_ERROR_NOENT))
        {
          g_clear_error (error);
          g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND, "Git is not installed");
        }
      return FALSE;
    }
  if (!g_subprocess_communicate_utf8 (proc, NULL, cancellable, &stdout_buf, &stderr_buf, error))
    return FALSE;
  if (!g_subprocess_get_successful (proc))
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED, "git %s failed: %s",
                   (const char *) argv->pdata[1], g_strstrip (stderr_buf ? stderr_buf : (char *) ""));
      return FALSE;
    }
  if (out)
    *out = g_strstrip (g_steal_pointer (&stdout_buf));
  return TRUE;
}

static gboolean
remove_tree (const char *path)
{
  g_autoptr (GFile) f = g_file_new_for_path (path);
  g_autoptr (GFileEnumerator) en = g_file_enumerate_children (f, G_FILE_ATTRIBUTE_STANDARD_NAME ","
                                                              G_FILE_ATTRIBUTE_STANDARD_TYPE,
                                                              G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS, NULL, NULL);
  GFileInfo *info;

  while (en && (info = g_file_enumerator_next_file (en, NULL, NULL)))
    {
      g_autofree char *child = g_build_filename (path, g_file_info_get_name (info), NULL);
      if (g_file_info_get_file_type (info) == G_FILE_TYPE_DIRECTORY)
        remove_tree (child);
      else
        g_unlink (child);
      g_object_unref (info);
    }
  return g_rmdir (path) == 0 || errno == ENOENT;
}

static void
sync_thread (GTask *task, gpointer source, gpointer task_data, GCancellable *cancellable)
{
  SyncData *d = task_data;
  g_autofree char *git_dir = g_build_filename (d->dir, ".git", NULL);
  g_autofree char *before = NULL, *after = NULL;
  GError *error = NULL;

  if (g_file_test (git_dir, G_FILE_TEST_IS_DIR))
    {
      /* update: fetch the newest commit and make the working copy match it exactly */
      git (cancellable, &before, NULL, "-C", d->dir, "rev-parse", "HEAD", NULL);
      if (!git (cancellable, NULL, &error, "-C", d->dir, "remote", "set-url", "origin", d->url, NULL) ||
          !git (cancellable, NULL, &error, "-C", d->dir, "fetch", "--quiet", "--depth", "1", "origin", "HEAD", NULL) ||
          !git (cancellable, NULL, &error, "-C", d->dir, "reset", "--quiet", "--hard", "FETCH_HEAD", NULL) ||
          !git (cancellable, NULL, &error, "-C", d->dir, "clean", "--quiet", "-fdx", NULL) ||
          !git (cancellable, &after, &error, "-C", d->dir, "rev-parse", "HEAD", NULL))
        {
          g_task_return_error (task, error);
          return;
        }
      g_task_return_int (task, g_strcmp0 (before, after) != 0);
      return;
    }

  /* first run: clone next to the final place, then move it in */
  {
    g_autofree char *tmp = g_strconcat (d->dir, ".new", NULL);
    g_autofree char *parent = g_path_get_dirname (d->dir);

    g_mkdir_with_parents (parent, 0700);
    remove_tree (tmp);
    if (!git (cancellable, NULL, &error, "clone", "--quiet", "--depth", "1", "--filter=blob:none", "--sparse",
              d->url, tmp, NULL) ||
        !git (cancellable, NULL, &error, "-C", tmp, "sparse-checkout", "set", "catalog", NULL))
      {
        remove_tree (tmp);
        g_task_return_error (task, error);
        return;
      }
    remove_tree (d->dir);
    if (g_rename (tmp, d->dir) != 0)
      {
        int saved = errno;
        g_task_return_new_error (task, G_IO_ERROR, g_io_error_from_errno (saved),
                                 "Cannot move the catalog into %s: %s", d->dir, g_strerror (saved));
        return;
      }
    g_task_return_int (task, TRUE);
  }
}

void
store_sync_async (const char *repo_url, const char *clone_dir, GCancellable *cancellable,
                  GAsyncReadyCallback callback, gpointer user_data)
{
  g_autoptr (GTask) task = g_task_new (NULL, cancellable, callback, user_data);
  SyncData *d = g_new0 (SyncData, 1);

  d->url = g_strdup (repo_url);
  d->dir = g_strdup (clone_dir);
  g_task_set_task_data (task, d, (GDestroyNotify) sync_data_free);
  g_task_set_source_tag (task, store_sync_async);
  g_task_run_in_thread (task, sync_thread);
}

gboolean
store_sync_finish (GAsyncResult *result, gboolean *changed, GError **error)
{
  GError *err = NULL;
  gssize r = g_task_propagate_int (G_TASK (result), &err);

  if (err)
    {
      g_propagate_error (error, err);
      return FALSE;
    }
  if (changed)
    *changed = r != 0;
  return TRUE;
}
