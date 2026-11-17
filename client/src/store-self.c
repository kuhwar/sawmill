/* Sawmill installs itself: on first run as an AppImage it moves into the standard install folder. */

#include <errno.h>
#include <string.h>
#include <glib/gstdio.h>
#include "store-self.h"
#include "store-install.h"

#define APP_ID "io.sawmill.Store"

static gboolean
same_file (const char *a, const char *b)
{
  g_autofree char *ca = g_canonicalize_filename (a, NULL);
  g_autofree char *cb = g_canonicalize_filename (b, NULL);
  GStatBuf sa, sb;

  if (g_str_equal (ca, cb))
    return TRUE;
  return g_stat (ca, &sa) == 0 && g_stat (cb, &sb) == 0 && sa.st_dev == sb.st_dev && sa.st_ino == sb.st_ino;
}

static gboolean
write_launcher (const char *desktop, const char *appimage, const char *icon, GError **error)
{
  g_autoptr (GKeyFile) kf = g_key_file_new ();
  g_autofree char *dir = g_path_get_dirname (desktop);
  g_autofree char *exec = g_strdup_printf ("\"%s\" %%U", appimage);
  const char *G = G_KEY_FILE_DESKTOP_GROUP;
  const char *keywords[] = { "app", "store", "software", "install", "appimage", NULL };
  const char *categories[] = { "GNOME", "GTK", "System", "PackageManager", NULL };

  g_key_file_set_string (kf, G, G_KEY_FILE_DESKTOP_KEY_TYPE, "Application");
  g_key_file_set_string (kf, G, G_KEY_FILE_DESKTOP_KEY_NAME, "Sawmill");
  g_key_file_set_string (kf, G, G_KEY_FILE_DESKTOP_KEY_COMMENT, "Find, install and update AppImages");
  g_key_file_set_string (kf, G, G_KEY_FILE_DESKTOP_KEY_EXEC, exec);
  g_key_file_set_string (kf, G, G_KEY_FILE_DESKTOP_KEY_TRY_EXEC, appimage);
  g_key_file_set_string (kf, G, G_KEY_FILE_DESKTOP_KEY_ICON, icon);
  g_key_file_set_boolean (kf, G, G_KEY_FILE_DESKTOP_KEY_TERMINAL, FALSE);
  g_key_file_set_string_list (kf, G, G_KEY_FILE_DESKTOP_KEY_CATEGORIES, categories, G_N_ELEMENTS (categories) - 1);
  g_key_file_set_string_list (kf, G, "Keywords", keywords, G_N_ELEMENTS (keywords) - 1);
  g_key_file_set_boolean (kf, G, G_KEY_FILE_DESKTOP_KEY_STARTUP_NOTIFY, TRUE);
  g_key_file_set_string (kf, G, "StartupWMClass", APP_ID);          /* match Sawmill's window to this launcher */
  g_key_file_set_string (kf, G, "X-Sawmill-Id", SAWMILL_SELF_ID);
  g_mkdir_with_parents (dir, 0755);
  return g_key_file_save_to_file (kf, desktop, error);
}

static gboolean
copy_file (const char *from, const char *to, GError **error)
{
  g_autoptr (GFile) a = g_file_new_for_path (from);
  g_autoptr (GFile) b = g_file_new_for_path (to);
  return g_file_copy (a, b, G_FILE_COPY_OVERWRITE, NULL, NULL, NULL, error);
}

static void
self_install_thread (GTask *task, gpointer source, gpointer data, GCancellable *cancel)
{
  const char *running = g_getenv ("APPIMAGE");
  const char *appdir = g_getenv ("APPDIR");
  g_autofree char *dir = store_install_dir_for (SAWMILL_SELF_ID);
  g_autofree char *target = g_build_filename (dir, "app.AppImage", NULL);
  g_autofree char *part = g_strconcat (target, ".part", NULL);
  g_autofree char *icon = g_build_filename (dir, "icon.png", NULL);
  g_autofree char *bundled_icon = appdir ? g_build_filename (appdir, APP_ID ".png", NULL) : NULL;
  g_autofree char *version = g_build_filename (dir, "version", NULL);
  g_autofree char *desktop = store_install_desktop_path_for (SAWMILL_SELF_ID);
  gboolean moved = FALSE;
  GError *error = NULL;

  if (!running || !*running || g_strcmp0 (g_getenv ("SAWMILL_NO_SELF_INSTALL"), "1") == 0)
    {
      g_task_return_pointer (task, NULL, NULL);
      return;
    }

  if (g_mkdir_with_parents (dir, 0755) != 0)
    {
      g_set_error (&error, G_IO_ERROR, g_io_error_from_errno (errno), "Cannot create %s: %s", dir, g_strerror (errno));
      g_task_return_error (task, error);
      return;
    }

  if (!same_file (running, target))
    {
      /* move: rename within one file system, otherwise copy and delete the original */
      if (g_rename (running, target) != 0)
        {
          if (!copy_file (running, part, &error) || g_chmod (part, 0755) != 0 || g_rename (part, target) != 0)
            {
              g_unlink (part);
              if (!error)
                g_set_error (&error, G_IO_ERROR, g_io_error_from_errno (errno), "Cannot save %s: %s", target, g_strerror (errno));
              g_task_return_error (task, error);
              return;
            }
          g_unlink (running);       /* best effort: the original may be on read-only media */
        }
      g_chmod (target, 0755);
      if (*SAWMILL_RELEASE_TAG)
        g_file_set_contents (version, SAWMILL_RELEASE_TAG, -1, NULL);
      moved = TRUE;
    }

  /* (re)create the icon and launcher every run, so a deleted launcher comes back */
  if (bundled_icon && g_file_test (bundled_icon, G_FILE_TEST_IS_REGULAR))
    copy_file (bundled_icon, icon, NULL);
  if (!write_launcher (desktop, target, g_file_test (icon, G_FILE_TEST_EXISTS) ? icon : APP_ID, &error))
    {
      g_task_return_error (task, error);
      return;
    }

  if (moved)
    {
      const char *home = g_get_home_dir ();
      g_autofree char *shown = g_str_has_prefix (dir, home) ? g_strconcat ("~", dir + strlen (home), NULL) : g_strdup (dir);
      g_task_return_pointer (task, g_strdup_printf ("Sawmill was installed in %s", shown), g_free);
    }
  else
    g_task_return_pointer (task, NULL, NULL);
}

void
store_self_install_async (GAsyncReadyCallback callback, gpointer user_data)
{
  g_autoptr (GTask) task = g_task_new (NULL, NULL, callback, user_data);
  g_task_set_source_tag (task, store_self_install_async);
  g_task_run_in_thread (task, self_install_thread);
}

char *
store_self_install_finish (GAsyncResult *result, GError **error)
{
  return g_task_propagate_pointer (G_TASK (result), error);
}
