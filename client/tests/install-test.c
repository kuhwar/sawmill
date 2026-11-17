/* Manual end-to-end test of the installer (needs the network):
 *   install-test <catalog-dir> <app-id> [--keep]
 * Installs the app, prints the files it created, then uninstalls it unless --keep is given. */

#include <gtk/gtk.h>
#include "store-catalog.h"
#include "store-install.h"

static GMainLoop *loop;
static gboolean ok;

static void
progress (StoreApp *a, const char *phase, double fraction, gpointer data)
{
  static char last[64];
  static int last_pct = -10;
  int pct = fraction < 0 ? -1 : (int) (fraction * 100);
  if (g_strcmp0 (last, phase) != 0 || pct >= last_pct + 10)
    {
      g_print ("  %s %s\n", phase, pct < 0 ? "" : g_strdup_printf ("%d%%", pct));
      g_strlcpy (last, phase, sizeof last);
      last_pct = pct;
    }
}

static void
done (GObject *src, GAsyncResult *res, gpointer data)
{
  g_autoptr (GError) error = NULL;
  g_autofree char *tag = store_install_finish (res, &error);
  if (tag)
    g_print ("installed %s\n", tag);
  else
    g_print ("FAILED: %s\n", error->message);
  ok = tag != NULL;
  g_main_loop_quit (loop);
}

int
main (int argc, char **argv)
{
  g_autoptr (GError) error = NULL;
  StoreCatalog *cat;
  StoreApp *a;

  gtk_init ();
  if (argc < 3)
    {
      g_printerr ("usage: %s <catalog-dir> <app-id> [--keep]\n", argv[0]);
      return 2;
    }
  if (!(cat = store_catalog_load (argv[1], &error)))
    {
      g_printerr ("%s\n", error->message);
      return 1;
    }
  if (!(a = store_catalog_find (cat, argv[2])))
    {
      g_printerr ("no app %s in the catalog\n", argv[2]);
      return 1;
    }

  loop = g_main_loop_new (NULL, FALSE);
  a->busy = TRUE;
  {
    /* SAWMILL_TEST_CANCEL_MS=<n>: cancel the install after n milliseconds */
    const char *cancel_ms = g_getenv ("SAWMILL_TEST_CANCEL_MS");
    GCancellable *cancel = g_cancellable_new ();
    if (cancel_ms)
      g_timeout_add (atoi (cancel_ms), (GSourceFunc) (void (*) (void)) g_cancellable_cancel, cancel);
    store_install_async (a, cancel, progress, NULL, done, NULL);
  }
  g_main_loop_run (loop);
  if (!ok)
    return 1;

  store_install_load_state (a);
  g_print ("state: %s, version file: %s\n", a->state == APP_INSTALLED ? "installed" : "NOT INSTALLED",
           a->installed_version ? a->installed_version : "(none)");
  {
    g_autofree char *dir = store_install_dir (a);
    g_autofree char *desktop = store_install_desktop_path (a);
    g_autofree char *cmd = g_strdup_printf ("ls -la '%s'; echo '--- %s'; cat '%s'", dir, desktop, desktop);
    system (cmd);
  }
  if (argc > 3 && g_str_equal (argv[3], "--keep"))
    return 0;

  if (!store_uninstall (a, &error))
    {
      g_printerr ("uninstall failed: %s\n", error->message);
      return 1;
    }
  {
    g_autofree char *dir = store_install_dir (a);
    g_autofree char *desktop = store_install_desktop_path (a);
    g_print ("after uninstall: folder %s, launcher %s, state %s\n",
             g_file_test (dir, G_FILE_TEST_EXISTS) ? "STILL THERE" : "removed",
             g_file_test (desktop, G_FILE_TEST_EXISTS) ? "STILL THERE" : "removed",
             a->state == APP_AVAILABLE ? "available" : "WRONG");
  }
  return 0;
}
