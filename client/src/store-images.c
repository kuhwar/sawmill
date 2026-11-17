/* Remote screenshots: download once with libsoup, keep a copy in the cache, decode off the main thread. */

#include <adwaita.h>
#include <libsoup/soup.h>
#include "store-images.h"

static SoupSession *
session (void)
{
  static SoupSession *s;
  if (!s)
    s = soup_session_new_with_options ("user-agent", "Sawmill/0.1 (AppImage store)", "timeout", 30, NULL);
  return s;
}

static char *
cache_path (const char *url)
{
  g_autofree char *hash = g_compute_checksum_for_string (G_CHECKSUM_SHA256, url, -1);
  return g_build_filename (g_get_user_cache_dir (), "sawmill", "images", hash, NULL);
}

/* ---------- decode in a thread ---------- */

static void
decode_thread (GTask *task, gpointer source, gpointer data, GCancellable *cancellable)
{
  GBytes *bytes = data;
  GError *error = NULL;
  GdkTexture *tex = gdk_texture_new_from_bytes (bytes, &error);   /* PNG/JPEG/TIFF, else gdk-pixbuf (WebP, GIF) */

  if (tex)
    g_task_return_pointer (task, tex, g_object_unref);
  else
    g_task_return_error (task, error);
}

static void
decode_async (GBytes *bytes, GCancellable *cancellable, GAsyncReadyCallback cb, gpointer user_data)
{
  g_autoptr (GTask) task = g_task_new (NULL, cancellable, cb, user_data);
  g_task_set_task_data (task, g_bytes_ref (bytes), (GDestroyNotify) g_bytes_unref);
  g_task_run_in_thread (task, decode_thread);
}

/* ---------- widget ---------- */

typedef struct {
  GtkWidget    *stack;       /* strong ref while loading */
  GCancellable *cancel;
  char         *url;
} Load;

static void
load_free (Load *l)
{
  g_object_unref (l->stack);
  g_object_unref (l->cancel);
  g_free (l->url);
  g_free (l);
}

static void
show_error (Load *l, const char *why)
{
  g_debug ("screenshot %s: %s", l->url, why);
  gtk_stack_set_visible_child_name (GTK_STACK (l->stack), "error");
}

static void
decoded (GObject *src, GAsyncResult *res, gpointer user_data)
{
  Load *l = user_data;
  g_autoptr (GError) error = NULL;
  g_autoptr (GdkTexture) tex = g_task_propagate_pointer (G_TASK (res), &error);

  if (!g_cancellable_is_cancelled (l->cancel))
    {
      if (tex)
        {
          GtkWidget *pic = gtk_stack_get_child_by_name (GTK_STACK (l->stack), "image");
          gtk_picture_set_paintable (GTK_PICTURE (pic), GDK_PAINTABLE (tex));
          gtk_stack_set_visible_child_name (GTK_STACK (l->stack), "image");
        }
      else
        show_error (l, error->message);
    }
  load_free (l);
}

static void
downloaded (GObject *src, GAsyncResult *res, gpointer user_data)
{
  Load *l = user_data;
  g_autoptr (GError) error = NULL;
  g_autoptr (GBytes) bytes = soup_session_send_and_read_finish (SOUP_SESSION (src), res, &error);
  SoupMessage *msg = soup_session_get_async_result_message (SOUP_SESSION (src), res);

  if (g_cancellable_is_cancelled (l->cancel))
    {
      load_free (l);
      return;
    }
  if (!bytes || !msg || !SOUP_STATUS_IS_SUCCESSFUL (soup_message_get_status (msg)))
    {
      show_error (l, error ? error->message : "HTTP error");
      load_free (l);
      return;
    }

  {
    g_autofree char *path = cache_path (l->url);
    g_autofree char *dir = g_path_get_dirname (path);
    g_mkdir_with_parents (dir, 0700);
    g_file_set_contents (path, g_bytes_get_data (bytes, NULL), g_bytes_get_size (bytes), NULL);
  }
  decode_async (bytes, l->cancel, decoded, l);
}

static void
on_destroy (GtkWidget *w, GCancellable *cancel)
{
  g_cancellable_cancel (cancel);
}

GtkWidget *
store_remote_picture_new (const char *url, int height)
{
  GtkWidget *stack = gtk_stack_new ();
  GtkWidget *spinner = adw_spinner_new ();
  GtkWidget *pic = gtk_picture_new ();
  GtkWidget *err = gtk_label_new ("Screenshot unavailable");
  g_autofree char *path = cache_path (url);
  g_autofree char *cached = NULL;
  gsize len = 0;
  Load *l = g_new0 (Load, 1);

  gtk_widget_set_size_request (stack, -1, height);
  gtk_widget_set_hexpand (stack, TRUE);
  gtk_widget_set_size_request (spinner, 32, 32);
  gtk_widget_set_halign (spinner, GTK_ALIGN_CENTER);
  gtk_widget_set_valign (spinner, GTK_ALIGN_CENTER);
  gtk_picture_set_content_fit (GTK_PICTURE (pic), GTK_CONTENT_FIT_CONTAIN);
  gtk_picture_set_can_shrink (GTK_PICTURE (pic), TRUE);
  gtk_widget_add_css_class (err, "dim-label");
  gtk_stack_add_named (GTK_STACK (stack), spinner, "loading");
  gtk_stack_add_named (GTK_STACK (stack), pic, "image");
  gtk_stack_add_named (GTK_STACK (stack), err, "error");
  gtk_stack_set_transition_type (GTK_STACK (stack), GTK_STACK_TRANSITION_TYPE_CROSSFADE);

  l->stack = g_object_ref (stack);
  l->cancel = g_cancellable_new ();
  l->url = g_strdup (url);
  g_signal_connect_object (stack, "destroy", G_CALLBACK (on_destroy), l->cancel, 0);

  if (g_file_get_contents (path, &cached, &len, NULL))
    {
      g_autoptr (GBytes) bytes = g_bytes_new_take (g_steal_pointer (&cached), len);
      decode_async (bytes, l->cancel, decoded, l);
    }
  else
    {
      g_autoptr (SoupMessage) msg = soup_message_new ("GET", url);
      if (!msg)
        {
          show_error (l, "bad URL");
          load_free (l);
          return stack;
        }
      soup_session_send_and_read_async (session (), msg, G_PRIORITY_LOW, l->cancel, downloaded, l);
    }
  return stack;
}
