/* Main window: Explore / Installed / Updates, app details, categories and search, fed by the catalog. */

#include <math.h>
#include "store-window.h"
#include "store-catalog.h"
#include "store-art.h"
#include "store-images.h"
#include "store-sync.h"

/* The app has a single window, so its widgets live in one static struct. */
typedef struct {
  GtkWidget          *window;
  AdwNavigationView  *nav;
  AdwToastOverlay    *toasts;
  AdwViewStack       *views;
  AdwViewStackPage   *updates_page;
  GtkStack           *content;           /* loading | error | browse | search */
  AdwStatusPage      *error_page;
  GtkWidget          *search_bar;
  GtkWidget          *search_entry;
  GtkWidget          *search_box;
  GtkWidget          *explore_box;
  GtkStack           *installed_stack;
  GtkWidget          *installed_box;
  GtkStack           *updates_stack;
  GtkWidget          *updates_box;
  AdwCarousel        *featured;
  GtkEventController *featured_motion;
  guint               carousel_timeout;
  GtkCssProvider     *banner_css;

  StoreCatalog       *catalog;
  GPtrArray          *old_catalogs;      /* kept alive: open pages may still point into them */
  char               *catalog_dir;
  char               *clone_dir;         /* NULL when SAWMILL_CATALOG_DIR is used */
  char               *repo_url;
  gboolean            syncing;
} Ui;

static Ui ui;

static void open_app      (StoreApp *a);
static void open_category (const StoreCategory *c);
static void refresh_lists (void);
static void start_sync    (void);

/* ---------- small helpers ---------- */

static void
clear_box (GtkWidget *box)
{
  GtkWidget *c;
  while ((c = gtk_widget_get_first_child (box)))
    gtk_box_remove (GTK_BOX (box), c);
}

static GtkWidget *
label_new (const char *text, const char *css, gboolean wrap)
{
  GtkWidget *l = gtk_label_new (text);
  gtk_label_set_xalign (GTK_LABEL (l), 0);
  if (css)
    {
      g_auto (GStrv) classes = g_strsplit (css, " ", -1);
      for (guint i = 0; classes[i]; i++)
        gtk_widget_add_css_class (l, classes[i]);
    }
  if (wrap)
    {
      gtk_label_set_wrap (GTK_LABEL (l), TRUE);
      gtk_label_set_wrap_mode (GTK_LABEL (l), PANGO_WRAP_WORD_CHAR);
    }
  return l;
}

static GtkWidget *
page_scroller (GtkWidget **out_box, int spacing)
{
  GtkWidget *sw = gtk_scrolled_window_new ();
  GtkWidget *clamp = adw_clamp_new ();
  GtkWidget *box = gtk_box_new (GTK_ORIENTATION_VERTICAL, spacing);

  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
  adw_clamp_set_maximum_size (ADW_CLAMP (clamp), 1000);
  adw_clamp_set_tightening_threshold (ADW_CLAMP (clamp), 700);
  gtk_widget_set_margin_top (box, 24);
  gtk_widget_set_margin_bottom (box, 36);
  gtk_widget_set_margin_start (box, 12);
  gtk_widget_set_margin_end (box, 12);
  adw_clamp_set_child (ADW_CLAMP (clamp), box);
  gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (sw), clamp);
  *out_box = box;
  return sw;
}

static GtkWidget *
section (const char *title, GtkWidget *child)
{
  GtkWidget *box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 12);
  gtk_box_append (GTK_BOX (box), label_new (title, "title-4", TRUE));
  gtk_box_append (GTK_BOX (box), child);
  return box;
}

static void
toast (const char *msg)
{
  adw_toast_overlay_add_toast (ui.toasts, adw_toast_new (msg));
}

static GtkWidget *
status_page (const char *icon, const char *title, const char *description)
{
  GtkWidget *s = adw_status_page_new ();
  adw_status_page_set_icon_name (ADW_STATUS_PAGE (s), icon);
  adw_status_page_set_title (ADW_STATUS_PAGE (s), title);
  adw_status_page_set_description (ADW_STATUS_PAGE (s), description);
  gtk_widget_set_vexpand (s, TRUE);
  return s;
}

static void
launch_uri (const char *uri)
{
  g_autoptr (GtkUriLauncher) l = gtk_uri_launcher_new (uri);
  gtk_uri_launcher_launch (l, GTK_WINDOW (ui.window), NULL, NULL, NULL);
}

static void on_link_clicked (GtkButton *b, gpointer data) { launch_uri (g_object_get_data (G_OBJECT (b), "uri")); }

static const char *
host_name (const char *host)
{
  if (g_str_equal (host, "github.com")) return "GitHub";
  if (g_str_equal (host, "gitlab.com")) return "GitLab";
  if (g_str_equal (host, "codeberg.org")) return "Codeberg";
  return host;
}

/* ---------- per-app watchers: widgets that update when an app's state changes ---------- */

typedef void (*WatchFn) (GtkWidget *w, StoreApp *a);
typedef struct { GtkWidget *widget; WatchFn fn; StoreApp *app; } Watch;

static void watch_destroyed (GtkWidget *w, Watch *wt) { g_ptr_array_remove (wt->app->watchers, wt); }

static void
watch (StoreApp *a, GtkWidget *w, WatchFn fn)
{
  Watch *wt = g_new0 (Watch, 1);
  wt->widget = w;
  wt->fn = fn;
  wt->app = a;
  g_ptr_array_add (a->watchers, wt);
  g_signal_connect (w, "destroy", G_CALLBACK (watch_destroyed), wt);
  fn (w, a);
}

static void
notify_app (StoreApp *a)
{
  for (guint i = 0; i < a->watchers->len; i++)
    {
      Watch *wt = g_ptr_array_index (a->watchers, i);
      wt->fn (wt->widget, a);
    }
}

/* ---------- install / update (simulated until AppImage installing is implemented) ---------- */

static gboolean
job_tick (gpointer data)
{
  StoreApp *a = data;

  a->progress = MIN (1.0, a->progress + 0.06 + g_random_double () * 0.08);
  if (a->progress >= 1.0)
    {
      g_autofree char *msg = g_strdup_printf (a->busy_is_update ? "%s was updated" : "%s is installed", a->title);
      a->busy = FALSE;
      a->state = APP_INSTALLED;
      notify_app (a);
      refresh_lists ();
      toast (msg);
      return G_SOURCE_REMOVE;
    }
  notify_app (a);
  return G_SOURCE_CONTINUE;
}

static void
start_job (StoreApp *a, gboolean is_update)
{
  if (a->busy)
    return;
  a->busy = TRUE;
  a->busy_is_update = is_update;
  a->progress = 0;
  g_timeout_add (200, job_tick, a);
  notify_app (a);
  refresh_lists ();
}

static void on_install (GtkButton *b, StoreApp *a) { start_job (a, FALSE); }
static void on_update  (GtkButton *b, StoreApp *a) { start_job (a, TRUE); }

static void
on_uninstall (GtkButton *b, StoreApp *a)
{
  g_autofree char *msg = g_strdup_printf ("%s was uninstalled", a->title);
  a->state = APP_AVAILABLE;
  notify_app (a);
  refresh_lists ();
  toast (msg);
}

static void
on_open (GtkButton *b, StoreApp *a)
{
  g_autofree char *msg = g_strdup_printf ("Launching %s is not implemented yet", a->title);
  toast (msg);
}

static void
on_update_all (GtkButton *b, gpointer data)
{
  for (guint i = 0; ui.catalog && i < ui.catalog->apps->len; i++)
    {
      StoreApp *a = g_ptr_array_index (ui.catalog->apps, i);
      if (a->state == APP_UPDATE)
        start_job (a, TRUE);
    }
}

/* ---------- tiles ---------- */

static void
update_pill (GtkWidget *w, StoreApp *a)
{
  gtk_widget_remove_css_class (w, "pill-ok");
  gtk_widget_remove_css_class (w, "pill-up");
  gtk_widget_set_visible (w, TRUE);
  if (a->busy)
    {
      gtk_label_set_text (GTK_LABEL (w), a->busy_is_update ? "Updating…" : "Installing…");
      gtk_widget_add_css_class (w, "pill-up");
    }
  else if (a->state == APP_INSTALLED)
    {
      gtk_label_set_text (GTK_LABEL (w), "Installed");
      gtk_widget_add_css_class (w, "pill-ok");
    }
  else if (a->state == APP_UPDATE)
    {
      gtk_label_set_text (GTK_LABEL (w), "Update available");
      gtk_widget_add_css_class (w, "pill-up");
    }
  else
    gtk_widget_set_visible (w, FALSE);
}

static void on_tile_clicked (GtkButton *b, StoreApp *a) { open_app (a); }

static GtkWidget *
tile_new (StoreApp *a)
{
  GtkWidget *btn = gtk_button_new ();
  GtkWidget *hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 14);
  GtkWidget *vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 2);
  GtkWidget *meta = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 8);
  GtkWidget *name, *sum, *dev, *pill;

  gtk_widget_add_css_class (btn, "card");
  gtk_widget_add_css_class (btn, "tile");
  gtk_box_append (GTK_BOX (hbox), store_app_icon_new (a, 64));

  name = label_new (a->title, "heading", FALSE);
  gtk_label_set_ellipsize (GTK_LABEL (name), PANGO_ELLIPSIZE_END);
  sum = label_new (a->summary, "dim-label", TRUE);
  gtk_label_set_lines (GTK_LABEL (sum), 2);
  gtk_label_set_ellipsize (GTK_LABEL (sum), PANGO_ELLIPSIZE_END);
  gtk_label_set_width_chars (GTK_LABEL (sum), 18);
  gtk_label_set_max_width_chars (GTK_LABEL (sum), 30);
  dev = label_new (a->developer, "caption dim-label", FALSE);
  gtk_label_set_ellipsize (GTK_LABEL (dev), PANGO_ELLIPSIZE_END);
  gtk_widget_set_hexpand (dev, TRUE);
  pill = label_new ("", "pill caption-heading", FALSE);
  watch (a, pill, update_pill);
  gtk_box_append (GTK_BOX (meta), dev);
  gtk_box_append (GTK_BOX (meta), pill);
  gtk_widget_set_margin_top (meta, 4);

  gtk_box_append (GTK_BOX (vbox), name);
  gtk_box_append (GTK_BOX (vbox), sum);
  gtk_box_append (GTK_BOX (vbox), meta);
  gtk_widget_set_valign (vbox, GTK_ALIGN_CENTER);
  gtk_widget_set_hexpand (vbox, TRUE);
  gtk_box_append (GTK_BOX (hbox), vbox);

  gtk_button_set_child (GTK_BUTTON (btn), hbox);
  gtk_accessible_update_property (GTK_ACCESSIBLE (btn), GTK_ACCESSIBLE_PROPERTY_LABEL, a->title, -1);
  g_signal_connect (btn, "clicked", G_CALLBACK (on_tile_clicked), a);
  return btn;
}

static GtkWidget *
flow_new (guint max_per_line)
{
  GtkWidget *fb = gtk_flow_box_new ();
  gtk_flow_box_set_selection_mode (GTK_FLOW_BOX (fb), GTK_SELECTION_NONE);
  gtk_flow_box_set_homogeneous (GTK_FLOW_BOX (fb), TRUE);
  gtk_flow_box_set_min_children_per_line (GTK_FLOW_BOX (fb), 1);
  gtk_flow_box_set_max_children_per_line (GTK_FLOW_BOX (fb), max_per_line);
  gtk_flow_box_set_column_spacing (GTK_FLOW_BOX (fb), 12);
  gtk_flow_box_set_row_spacing (GTK_FLOW_BOX (fb), 12);
  gtk_widget_set_valign (fb, GTK_ALIGN_START);
  return fb;
}

static void
flow_add (GtkWidget *fb, GtkWidget *child)
{
  gtk_flow_box_append (GTK_FLOW_BOX (fb), child);
  gtk_widget_set_focusable (gtk_widget_get_parent (child), FALSE);
}

static GtkWidget *
tiles_for (GPtrArray *apps, guint limit)
{
  GtkWidget *fb = flow_new (3);
  for (guint i = 0; i < apps->len && (limit == 0 || i < limit); i++)
    flow_add (fb, tile_new (g_ptr_array_index (apps, i)));
  return fb;
}

/* ---------- carousels ---------- */

static void
carousel_step (AdwCarousel *c, int delta)
{
  int n = (int) adw_carousel_get_n_pages (c);
  int pos = (int) round (adw_carousel_get_position (c));
  if (n < 2)
    return;
  adw_carousel_scroll_to (c, adw_carousel_get_nth_page (c, ((pos + delta) % n + n) % n), TRUE);
}

static void on_prev (GtkButton *b, AdwCarousel *c) { carousel_step (c, -1); }
static void on_next (GtkButton *b, AdwCarousel *c) { carousel_step (c, 1); }

static GtkWidget *
carousel_frame (AdwCarousel *c, guint n_pages, GtkEventController **motion_out)
{
  GtkWidget *box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 8);
  GtkWidget *overlay = gtk_overlay_new ();

  gtk_overlay_set_child (GTK_OVERLAY (overlay), GTK_WIDGET (c));
  if (n_pages > 1)
    {
      for (int i = 0; i < 2; i++)
        {
          GtkWidget *b = gtk_button_new_from_icon_name (i ? "go-next-symbolic" : "go-previous-symbolic");
          gtk_widget_add_css_class (b, "circular");
          gtk_widget_add_css_class (b, "osd");
          gtk_widget_set_valign (b, GTK_ALIGN_CENTER);
          gtk_widget_set_halign (b, i ? GTK_ALIGN_END : GTK_ALIGN_START);
          gtk_widget_set_margin_start (b, 12);
          gtk_widget_set_margin_end (b, 12);
          gtk_widget_set_tooltip_text (b, i ? "Next" : "Previous");
          g_signal_connect (b, "clicked", i ? G_CALLBACK (on_next) : G_CALLBACK (on_prev), c);
          gtk_overlay_add_overlay (GTK_OVERLAY (overlay), b);
        }
    }
  gtk_box_append (GTK_BOX (box), overlay);
  if (n_pages > 1)
    {
      GtkWidget *dots = adw_carousel_indicator_dots_new ();
      adw_carousel_indicator_dots_set_carousel (ADW_CAROUSEL_INDICATOR_DOTS (dots), c);
      gtk_box_append (GTK_BOX (box), dots);
    }
  if (motion_out)
    {
      *motion_out = gtk_event_controller_motion_new ();
      gtk_widget_add_controller (overlay, *motion_out);
    }
  return box;
}

static gboolean
auto_advance (gpointer data)
{
  if (ui.featured && ui.featured_motion && gtk_widget_get_mapped (GTK_WIDGET (ui.featured)) &&
      !gtk_event_controller_motion_contains_pointer (GTK_EVENT_CONTROLLER_MOTION (ui.featured_motion)))
    carousel_step (ui.featured, 1);
  return G_SOURCE_CONTINUE;
}

/* ---------- Explore ---------- */

static void
update_banner_css (void)
{
  GString *css = g_string_new (NULL);
  for (guint i = 0; ui.catalog && i < ui.catalog->banner->len; i++)
    {
      const GdkRGBA *c = store_app_get_accent (g_ptr_array_index (ui.catalog->banner, i));
      g_string_append_printf (css, "button.banner.banner-%u { background-image: linear-gradient(135deg, "
                              "rgb(%d,%d,%d), rgb(%d,%d,%d)); }\n", i,
                              (int) (c->red * 255), (int) (c->green * 255), (int) (c->blue * 255),
                              (int) (c->red * 150), (int) (c->green * 150), (int) (c->blue * 150));
    }
  gtk_css_provider_load_from_string (ui.banner_css, css->str);
  g_string_free (css, TRUE);
}

static GtkWidget *
banner_new (StoreApp *a, guint index)
{
  GtkWidget *btn = gtk_button_new ();
  GtkWidget *hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 24);
  GtkWidget *text = gtk_box_new (GTK_ORIENTATION_VERTICAL, 8);
  GtkWidget *icon = store_app_icon_new (a, 96);
  g_autofree char *cls = g_strdup_printf ("banner-%u", index);

  gtk_widget_add_css_class (btn, "banner");
  gtk_widget_add_css_class (btn, cls);
  gtk_widget_set_overflow (btn, GTK_OVERFLOW_HIDDEN);
  gtk_widget_set_hexpand (btn, TRUE);
  gtk_widget_set_size_request (btn, -1, 260);

  gtk_widget_set_halign (icon, GTK_ALIGN_START);
  gtk_box_append (GTK_BOX (text), icon);
  gtk_box_append (GTK_BOX (text), label_new (a->title, "banner-title", TRUE));
  gtk_box_append (GTK_BOX (text), label_new (a->summary, "banner-summary", TRUE));
  gtk_box_append (GTK_BOX (text), label_new (a->developer, "banner-developer", TRUE));
  gtk_widget_set_valign (text, GTK_ALIGN_CENTER);
  gtk_widget_set_hexpand (text, TRUE);
  gtk_widget_set_margin_start (text, 36);
  gtk_widget_set_margin_end (text, a->thumbnails[0] ? 0 : 36);
  gtk_widget_set_margin_top (text, 24);
  gtk_widget_set_margin_bottom (text, 24);
  gtk_box_append (GTK_BOX (hbox), text);

  if (a->thumbnails[0])
    {
      GtkWidget *shot = store_remote_picture_new (a->thumbnails[0], 220);
      gtk_widget_add_css_class (shot, "banner-shot");
      gtk_widget_set_valign (shot, GTK_ALIGN_CENTER);
      gtk_widget_set_margin_end (shot, 24);
      gtk_box_set_homogeneous (GTK_BOX (hbox), TRUE);
      gtk_box_append (GTK_BOX (hbox), shot);
    }

  gtk_button_set_child (GTK_BUTTON (btn), hbox);
  gtk_accessible_update_property (GTK_ACCESSIBLE (btn), GTK_ACCESSIBLE_PROPERTY_LABEL, a->title, -1);
  g_signal_connect (btn, "clicked", G_CALLBACK (on_tile_clicked), a);
  return btn;
}

static void on_category_clicked (GtkButton *b, StoreCategory *c) { open_category (c); }

static void
rebuild_explore (void)
{
  StoreCatalog *cat = ui.catalog;
  GtkWidget *box = ui.explore_box;
  GtkWidget *cats = flow_new (4);
  g_autofree char *all_title = NULL;

  clear_box (box);
  ui.featured = NULL;
  ui.featured_motion = NULL;
  update_banner_css ();

  if (cat->banner->len)
    {
      AdwCarousel *c = ADW_CAROUSEL (adw_carousel_new ());
      adw_carousel_set_spacing (c, 12);
      for (guint i = 0; i < cat->banner->len; i++)
        adw_carousel_append (c, banner_new (g_ptr_array_index (cat->banner, i), i));
      ui.featured = c;
      gtk_box_append (GTK_BOX (box), carousel_frame (c, cat->banner->len, &ui.featured_motion));
    }
  if (cat->picks->len)
    gtk_box_append (GTK_BOX (box), section ("Editor's Choice", tiles_for (cat->picks, 0)));

  for (guint i = 0; i < cat->categories->len; i++)
    {
      StoreCategory *c = g_ptr_array_index (cat->categories, i);
      GtkWidget *btn = gtk_button_new ();
      GtkWidget *hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 12);
      GtkWidget *glyph = store_glyph_icon_new (store_category_glyph (c->id), 22);

      gtk_widget_add_css_class (btn, "card");
      gtk_widget_add_css_class (btn, "category");
      gtk_widget_add_css_class (glyph, "accent");
      gtk_box_append (GTK_BOX (hbox), glyph);
      gtk_box_append (GTK_BOX (hbox), label_new (c->name, "heading", FALSE));
      gtk_button_set_child (GTK_BUTTON (btn), hbox);
      gtk_widget_set_tooltip_text (btn, c->description);
      g_signal_connect (btn, "clicked", G_CALLBACK (on_category_clicked), c);
      flow_add (cats, btn);
    }
  gtk_box_append (GTK_BOX (box), section ("Categories", cats));

  all_title = g_strdup_printf ("All Apps (%u)", cat->apps->len);
  gtk_box_append (GTK_BOX (box), section (all_title, tiles_for (cat->apps, 0)));
}

/* ---------- Installed & Updates ---------- */

static void on_row_activated (AdwActionRow *row, StoreApp *a) { open_app (a); }

static void
update_row_progress (GtkWidget *w, StoreApp *a)
{
  if (a->busy)
    gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (w), a->progress);
}

static GtkWidget *
app_row_new (StoreApp *a, gboolean for_updates)
{
  GtkWidget *row = adw_action_row_new ();

  adw_preferences_row_set_use_markup (ADW_PREFERENCES_ROW (row), FALSE);
  adw_preferences_row_set_title (ADW_PREFERENCES_ROW (row), a->title);
  adw_action_row_set_subtitle (ADW_ACTION_ROW (row), a->busy ? (a->busy_is_update ? "Updating…" : "Installing…")
                                                              : a->developer);
  adw_action_row_add_prefix (ADW_ACTION_ROW (row), store_app_icon_new (a, 48));
  if (a->busy)
    {
      GtkWidget *pb = gtk_progress_bar_new ();
      gtk_widget_set_valign (pb, GTK_ALIGN_CENTER);
      gtk_widget_set_size_request (pb, 100, -1);
      watch (a, pb, update_row_progress);
      adw_action_row_add_suffix (ADW_ACTION_ROW (row), pb);
    }
  else
    {
      GtkWidget *btn = gtk_button_new_with_label (for_updates ? "Update" : "Uninstall");
      gtk_widget_set_valign (btn, GTK_ALIGN_CENTER);
      g_signal_connect (btn, "clicked", for_updates ? G_CALLBACK (on_update) : G_CALLBACK (on_uninstall), a);
      adw_action_row_add_suffix (ADW_ACTION_ROW (row), btn);
    }
  gtk_list_box_row_set_activatable (GTK_LIST_BOX_ROW (row), TRUE);
  g_signal_connect (row, "activated", G_CALLBACK (on_row_activated), a);
  return row;
}

static GtkWidget *
boxed_list_new (void)
{
  GtkWidget *list = gtk_list_box_new ();
  gtk_list_box_set_selection_mode (GTK_LIST_BOX (list), GTK_SELECTION_NONE);
  gtk_widget_add_css_class (list, "boxed-list");
  return list;
}

static void
refresh_lists (void)
{
  GtkWidget *installed = boxed_list_new (), *updates = boxed_list_new ();
  guint n_installed = 0, n_updates = 0;

  clear_box (ui.installed_box);
  clear_box (ui.updates_box);
  for (guint i = 0; ui.catalog && i < ui.catalog->apps->len; i++)
    {
      StoreApp *a = g_ptr_array_index (ui.catalog->apps, i);
      if (a->state != APP_AVAILABLE || a->busy)
        {
          gtk_list_box_append (GTK_LIST_BOX (installed), app_row_new (a, FALSE));
          n_installed++;
        }
      if (a->state == APP_UPDATE)
        {
          gtk_list_box_append (GTK_LIST_BOX (updates), app_row_new (a, TRUE));
          n_updates++;
        }
    }

  if (n_installed)
    {
      g_autofree char *t = g_strdup_printf ("%u Installed App%s", n_installed, n_installed == 1 ? "" : "s");
      gtk_box_append (GTK_BOX (ui.installed_box), label_new (t, "title-4", FALSE));
      gtk_box_append (GTK_BOX (ui.installed_box), installed);
      gtk_stack_set_visible_child_name (ui.installed_stack, "list");
    }
  else
    {
      g_object_ref_sink (installed);
      g_object_unref (installed);
      gtk_stack_set_visible_child_name (ui.installed_stack, "empty");
    }

  adw_view_stack_page_set_badge_number (ui.updates_page, n_updates);
  adw_view_stack_page_set_needs_attention (ui.updates_page, n_updates > 0);
  if (n_updates)
    {
      GtkWidget *head = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 12);
      GtkWidget *btn = gtk_button_new_with_label ("Update All");
      g_autofree char *t = g_strdup_printf ("%u Update%s", n_updates, n_updates == 1 ? "" : "s");
      GtkWidget *title = label_new (t, "title-4", FALSE);
      gtk_widget_set_hexpand (title, TRUE);
      gtk_widget_add_css_class (btn, "suggested-action");
      g_signal_connect (btn, "clicked", G_CALLBACK (on_update_all), NULL);
      gtk_box_append (GTK_BOX (head), title);
      gtk_box_append (GTK_BOX (head), btn);
      gtk_box_append (GTK_BOX (ui.updates_box), head);
      gtk_box_append (GTK_BOX (ui.updates_box), updates);
      gtk_stack_set_visible_child_name (ui.updates_stack, "list");
    }
  else
    {
      g_object_ref_sink (updates);
      g_object_unref (updates);
      gtk_stack_set_visible_child_name (ui.updates_stack, "empty");
    }
}

/* ---------- App details ---------- */

static void
update_action (GtkWidget *box, StoreApp *a)
{
  GtkWidget *first = gtk_widget_get_first_child (box);

  if (a->busy && first && GTK_IS_PROGRESS_BAR (first))
    {
      g_autofree char *t = g_strdup_printf ("%s %d%%", a->busy_is_update ? "Updating" : "Installing", (int) (a->progress * 100));
      gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (first), a->progress);
      gtk_progress_bar_set_text (GTK_PROGRESS_BAR (first), t);
      return;
    }
  clear_box (box);
  if (a->busy)
    {
      GtkWidget *pb = gtk_progress_bar_new ();
      gtk_progress_bar_set_show_text (GTK_PROGRESS_BAR (pb), TRUE);
      gtk_progress_bar_set_text (GTK_PROGRESS_BAR (pb), a->busy_is_update ? "Updating 0%" : "Installing 0%");
      gtk_widget_set_size_request (pb, 180, -1);
      gtk_widget_set_valign (pb, GTK_ALIGN_CENTER);
      gtk_box_append (GTK_BOX (box), pb);
    }
  else if (a->state == APP_INSTALLED)
    {
      GtkWidget *open = gtk_button_new_with_label ("Open");
      GtkWidget *rm = gtk_button_new_with_label ("Uninstall");
      gtk_widget_add_css_class (open, "pill");
      gtk_widget_add_css_class (rm, "pill");
      gtk_widget_add_css_class (rm, "destructive-action");
      g_signal_connect (open, "clicked", G_CALLBACK (on_open), a);
      g_signal_connect (rm, "clicked", G_CALLBACK (on_uninstall), a);
      gtk_box_append (GTK_BOX (box), open);
      gtk_box_append (GTK_BOX (box), rm);
    }
  else
    {
      gboolean upd = a->state == APP_UPDATE;
      GtkWidget *btn = gtk_button_new_with_label (upd ? "Update" : "Install");
      gtk_widget_add_css_class (btn, "pill");
      gtk_widget_add_css_class (btn, "suggested-action");
      g_signal_connect (btn, "clicked", upd ? G_CALLBACK (on_update) : G_CALLBACK (on_install), a);
      gtk_box_append (GTK_BOX (box), btn);
    }
}

static GtkWidget *
context_tile (GtkWidget *content, const char *tone, const char *title, const char *caption)
{
  GtkWidget *tile = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
  GtkWidget *lz = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  GtkWidget *t = label_new (title, "heading", TRUE);
  GtkWidget *c = label_new (caption, "dim-label caption", TRUE);

  gtk_widget_add_css_class (tile, "card");
  gtk_widget_add_css_class (tile, "context-tile");
  gtk_widget_add_css_class (lz, "lozenge");
  gtk_widget_add_css_class (lz, tone);
  gtk_widget_set_halign (lz, GTK_ALIGN_CENTER);
  gtk_widget_set_hexpand (content, TRUE);
  gtk_widget_set_halign (content, GTK_ALIGN_CENTER);
  gtk_widget_set_valign (content, GTK_ALIGN_CENTER);
  gtk_box_append (GTK_BOX (lz), content);
  for (int i = 0; i < 2; i++)
    {
      GtkWidget *l = i ? c : t;
      gtk_label_set_xalign (GTK_LABEL (l), 0.5);
      gtk_label_set_justify (GTK_LABEL (l), GTK_JUSTIFY_CENTER);
    }
  gtk_widget_set_margin_top (t, 4);
  gtk_box_append (GTK_BOX (tile), lz);
  gtk_box_append (GTK_BOX (tile), t);
  gtk_box_append (GTK_BOX (tile), c);
  return tile;
}

static GtkWidget *
icon_image (const char *name)
{
  GtkWidget *img = gtk_image_new_from_icon_name (name);
  gtk_image_set_pixel_size (GTK_IMAGE (img), 18);
  return img;
}

/* Paragraphs separated by blank lines; lines starting with "- " become bullets. */
static GtkWidget *
description_widget (const char *text)
{
  GtkWidget *box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 10);
  g_auto (GStrv) lines = g_strsplit (text, "\n", -1);
  GString *para = g_string_new (NULL);

  for (guint i = 0;; i++)
    {
      const char *line = lines[i] ? g_strstrip (lines[i]) : NULL;
      gboolean bullet = line && g_str_has_prefix (line, "- ");
      if ((!line || !*line || bullet) && para->len)
        {
          gtk_box_append (GTK_BOX (box), label_new (para->str, NULL, TRUE));
          g_string_truncate (para, 0);
        }
      if (!line)
        break;
      if (bullet)
        {
          g_autofree char *b = g_strdup_printf ("•  %s", line + 2);
          GtkWidget *l = label_new (b, NULL, TRUE);
          gtk_widget_set_margin_start (l, 8);
          gtk_box_append (GTK_BOX (box), l);
        }
      else if (*line)
        {
          if (para->len)
            g_string_append_c (para, ' ');
          g_string_append (para, line);
        }
    }
  g_string_free (para, TRUE);
  return box;
}

static const char *
age_caption (const char *age)
{
  if (g_str_equal (age, "all")) return "Suitable for everyone";
  if (g_str_equal (age, "7+")) return "Ages 7 and up";
  if (g_str_equal (age, "12+")) return "Ages 12 and up";
  if (g_str_equal (age, "16+")) return "Ages 16 and up";
  if (g_str_equal (age, "18+")) return "Adults only";
  return age;
}

static GtkWidget *
link_button (const char *label, const char *icon, const char *uri)
{
  GtkWidget *b = gtk_button_new ();
  GtkWidget *content = adw_button_content_new ();
  adw_button_content_set_label (ADW_BUTTON_CONTENT (content), label);
  adw_button_content_set_icon_name (ADW_BUTTON_CONTENT (content), icon);
  gtk_button_set_child (GTK_BUTTON (b), content);
  gtk_widget_add_css_class (b, "pill");
  gtk_widget_set_tooltip_text (b, uri);
  g_object_set_data_full (G_OBJECT (b), "uri", g_strdup (uri), g_free);
  g_signal_connect (b, "clicked", G_CALLBACK (on_link_clicked), NULL);
  return b;
}

static void
push_page (const char *title, GtkWidget *content)
{
  GtkWidget *tv = adw_toolbar_view_new ();
  adw_toolbar_view_add_top_bar (ADW_TOOLBAR_VIEW (tv), adw_header_bar_new ());
  adw_toolbar_view_set_content (ADW_TOOLBAR_VIEW (tv), content);
  adw_navigation_view_push (ui.nav, adw_navigation_page_new (tv, title));
}

static void
open_app (StoreApp *a)
{
  StoreCatalog *cat = ui.catalog;
  GtkWidget *box, *sw = page_scroller (&box, 32);
  GtkWidget *hero = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 24);
  GtkWidget *info = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
  GtkWidget *actions = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
  GtkWidget *action = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 8);
  GtkWidget *desc, *ctx, *links, *source, *more;
  GString *cats = g_string_new (NULL);
  g_autofree char *src_url = store_app_source_url (a);
  g_autofree char *src_caption = g_strdup_printf ("AppImage · %s/%s", a->owner, a->repo);
  g_autofree char *hosted = g_strdup_printf ("Hosted on %s", host_name (a->host));
  g_autofree char *repo_caption = g_strdup_printf ("%s/%s", a->owner, a->repo);
  const char *first_cat = a->categories[0];
  g_autofree char *more_title = g_strdup_printf ("More in %s", store_catalog_category_name (cat, first_cat));
  gboolean free_sw = store_app_is_free_software (a);
  gboolean unknown_license = g_str_equal (a->license, "LicenseRef-unknown");
  guint n_more = 0;

  /* hero */
  gtk_box_append (GTK_BOX (hero), store_app_icon_new (a, 128));
  gtk_box_append (GTK_BOX (info), label_new (a->title, "title-1", TRUE));
  for (guint i = 0; a->categories[i]; i++)
    g_string_append_printf (cats, "%s%s", i ? ", " : "", store_catalog_category_name (cat, a->categories[i]));
  {
    g_autofree char *line = g_strdup_printf ("%s · %s", a->developer, cats->str);
    gtk_box_append (GTK_BOX (info), label_new (line, "dim-label", TRUE));
  }
  g_string_free (cats, TRUE);
  gtk_widget_set_valign (info, GTK_ALIGN_CENTER);
  gtk_widget_set_hexpand (info, TRUE);
  gtk_box_append (GTK_BOX (hero), info);

  gtk_widget_set_halign (action, GTK_ALIGN_CENTER);
  watch (a, action, update_action);
  source = label_new (src_caption, "dim-label caption", FALSE);
  gtk_label_set_xalign (GTK_LABEL (source), 0.5);
  gtk_label_set_ellipsize (GTK_LABEL (source), PANGO_ELLIPSIZE_MIDDLE);
  gtk_box_append (GTK_BOX (actions), action);
  gtk_box_append (GTK_BOX (actions), source);
  gtk_widget_set_valign (actions, GTK_ALIGN_CENTER);
  gtk_box_append (GTK_BOX (hero), actions);
  gtk_box_append (GTK_BOX (box), hero);

  /* screenshots */
  if (a->thumbnails[0])
    {
      AdwCarousel *shots = ADW_CAROUSEL (adw_carousel_new ());
      guint n = g_strv_length (a->thumbnails);
      GtkWidget *frame;
      adw_carousel_set_spacing (shots, 24);
      for (guint i = 0; i < n; i++)
        adw_carousel_append (shots, store_remote_picture_new (a->thumbnails[i], 380));
      frame = carousel_frame (shots, n, NULL);
      gtk_widget_add_css_class (frame, "shots-frame");
      gtk_box_append (GTK_BOX (box), frame);
    }

  /* description */
  desc = gtk_box_new (GTK_ORIENTATION_VERTICAL, 10);
  gtk_box_append (GTK_BOX (desc), label_new (a->summary, "title-2", TRUE));
  if (*a->description)
    gtk_box_append (GTK_BOX (desc), description_widget (a->description));
  gtk_box_append (GTK_BOX (box), desc);

  /* context tiles */
  ctx = flow_new (4);
  gtk_flow_box_set_min_children_per_line (GTK_FLOW_BOX (ctx), 2);
  flow_add (ctx, context_tile (gtk_label_new (g_str_equal (a->age_rating, "all") ? "All" : a->age_rating),
                               "lz-neutral", "Age Rating", age_caption (a->age_rating)));
  flow_add (ctx, context_tile (icon_image (free_sw ? "security-high-symbolic" : "dialog-question-symbolic"),
                               free_sw ? "lz-ok" : "lz-warn",
                               free_sw ? "Free Software" : unknown_license ? "Unknown License" : "Proprietary",
                               free_sw ? a->license : unknown_license ? "Check the project's license" : "Source code is not available"));
  flow_add (ctx, context_tile (icon_image ("security-low-symbolic"), "lz-warn", "Not Sandboxed",
                               "Runs with full access to your files and devices"));
  {
    GtkWidget *glyph = store_glyph_icon_new (GLYPH_CODE, 18);
    flow_add (ctx, context_tile (glyph, "lz-blue", hosted, repo_caption));
  }
  gtk_box_append (GTK_BOX (box), ctx);

  /* links */
  links = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 12);
  gtk_widget_set_halign (links, GTK_ALIGN_START);
  if (a->homepage)
    gtk_box_append (GTK_BOX (links), link_button ("Website", "web-browser-symbolic", a->homepage));
  gtk_box_append (GTK_BOX (links), link_button ("Source Code", "folder-download-symbolic", src_url));
  gtk_box_append (GTK_BOX (box), links);

  /* more apps in the first category */
  more = flow_new (3);
  for (guint i = 0; i < cat->apps->len && n_more < 6; i++)
    {
      StoreApp *o = g_ptr_array_index (cat->apps, i);
      if (o != a && g_strv_contains ((const char * const *) o->categories, first_cat))
        {
          flow_add (more, tile_new (o));
          n_more++;
        }
    }
  if (n_more)
    gtk_box_append (GTK_BOX (box), section (more_title, more));
  else
    {
      g_object_ref_sink (more);
      g_object_unref (more);
    }

  push_page (a->title, sw);
}

static void
open_category (const StoreCategory *c)
{
  GtkWidget *box, *sw = page_scroller (&box, 24);
  GtkWidget *fb = flow_new (3);
  GtkWidget *title = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 12);
  GtkWidget *glyph = store_glyph_icon_new (store_category_glyph (c->id), 32);
  guint n = 0;
  g_autofree char *count = NULL;

  for (guint i = 0; i < ui.catalog->apps->len; i++)
    {
      StoreApp *a = g_ptr_array_index (ui.catalog->apps, i);
      if (g_strv_contains ((const char * const *) a->categories, c->id))
        {
          flow_add (fb, tile_new (a));
          n++;
        }
    }
  count = g_strdup_printf ("%u app%s", n, n == 1 ? "" : "s");
  gtk_widget_add_css_class (glyph, "accent");
  gtk_box_append (GTK_BOX (title), glyph);
  gtk_box_append (GTK_BOX (title), label_new (c->name, "title-1", FALSE));
  gtk_box_append (GTK_BOX (box), title);
  if (*c->description)
    gtk_box_append (GTK_BOX (box), label_new (c->description, "dim-label", TRUE));
  gtk_box_append (GTK_BOX (box), section (count, fb));
  push_page (c->name, sw);
}

/* ---------- Search ---------- */

static void
run_search (void)
{
  const char *q = gtk_editable_get_text (GTK_EDITABLE (ui.search_entry));
  g_autoptr (GPtrArray) hits = NULL;

  if (!ui.catalog || !q || !*q)
    {
      if (ui.catalog)
        gtk_stack_set_visible_child_name (ui.content, "browse");
      return;
    }
  clear_box (ui.search_box);
  hits = store_catalog_search (ui.catalog, q);
  if (hits->len == 0)
    gtk_box_append (GTK_BOX (ui.search_box),
                    status_page ("system-search-symbolic", "No Results Found",
                                 "Try a different name, or a word like “notes”, “video” or “browser”."));
  else
    {
      g_autofree char *t = g_strdup_printf ("%u result%s", hits->len, hits->len == 1 ? "" : "s");
      gtk_box_append (GTK_BOX (ui.search_box), section (t, tiles_for (hits, 0)));
    }
  gtk_stack_set_visible_child_name (ui.content, "search");
}

static void on_search_changed (GtkSearchEntry *e, gpointer d) { run_search (); }

static void
on_search_mode (GObject *bar, GParamSpec *pspec, gpointer data)
{
  if (!gtk_search_bar_get_search_mode (GTK_SEARCH_BAR (bar)))
    gtk_editable_set_text (GTK_EDITABLE (ui.search_entry), "");
}

static void
on_view_changed (GObject *stack, GParamSpec *pspec, gpointer data)
{
  gtk_search_bar_set_search_mode (GTK_SEARCH_BAR (ui.search_bar), FALSE);
}

void
store_window_toggle_search (void)
{
  if (!ui.search_bar)
    return;
  adw_navigation_view_pop_to_tag (ui.nav, "main");
  gtk_search_bar_set_search_mode (GTK_SEARCH_BAR (ui.search_bar),
                                  !gtk_search_bar_get_search_mode (GTK_SEARCH_BAR (ui.search_bar)));
}

/* ---------- Catalog loading and sync ---------- */

static gboolean
load_catalog (GError **error)
{
  StoreCatalog *cat = store_catalog_load (ui.catalog_dir, error);
  if (!cat)
    return FALSE;
  if (ui.catalog)
    g_ptr_array_add (ui.old_catalogs, ui.catalog);    /* open pages may still use it */
  ui.catalog = cat;
  rebuild_explore ();
  refresh_lists ();
  if (gtk_search_bar_get_search_mode (GTK_SEARCH_BAR (ui.search_bar)) &&
      *gtk_editable_get_text (GTK_EDITABLE (ui.search_entry)))
    run_search ();
  else
    gtk_stack_set_visible_child_name (ui.content, "browse");
  if (cat->skipped)
    {
      g_autofree char *msg = g_strdup_printf ("%u catalog entr%s could not be read", cat->skipped,
                                              cat->skipped == 1 ? "y" : "ies");
      toast (msg);
    }
  return TRUE;
}

static void
show_error (const char *title, const char *description)
{
  adw_status_page_set_title (ui.error_page, title);
  adw_status_page_set_description (ui.error_page, description);
  gtk_stack_set_visible_child_name (ui.content, "error");
}

static void
sync_done (GObject *src, GAsyncResult *res, gpointer data)
{
  g_autoptr (GError) error = NULL;
  gboolean changed = FALSE;

  ui.syncing = FALSE;
  if (!ui.window)
    return;
  if (!store_sync_finish (res, &changed, &error))
    {
      g_autofree char *msg = NULL;
      if (ui.catalog)
        {
          msg = g_strdup_printf ("Could not update the catalog: %s", error->message);
          toast (msg);
        }
      else if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND))
        show_error ("Git Is Required", "Sawmill uses Git to download its catalog. Install the git package and try again.");
      else
        {
          msg = g_strdup_printf ("Check your internet connection and try again.\n\n%s", error->message);
          show_error ("Could Not Download the Catalog", msg);
        }
      return;
    }
  if (changed || !ui.catalog)
    {
      g_autoptr (GError) lerr = NULL;
      gboolean had = ui.catalog != NULL;
      if (!load_catalog (&lerr))
        {
          if (g_error_matches (lerr, STORE_CATALOG_ERROR, STORE_CATALOG_ERROR_TOO_NEW))
            show_error ("Update Sawmill", lerr->message);
          else
            show_error ("The Catalog Could Not Be Read", lerr->message);
        }
      else if (had)
        toast ("Catalog updated");
    }
}

static void
start_sync (void)
{
  if (!ui.clone_dir || ui.syncing)
    return;
  ui.syncing = TRUE;
  if (!ui.catalog)
    gtk_stack_set_visible_child_name (ui.content, "loading");
  store_sync_async (ui.repo_url, ui.clone_dir, NULL, sync_done, NULL);
}

void
store_window_refresh_catalog (void)
{
  if (!ui.clone_dir)
    {
      g_autoptr (GError) error = NULL;
      if (!load_catalog (&error))
        show_error ("The Catalog Could Not Be Read", error->message);
      else
        toast ("Catalog reloaded");
      return;
    }
  start_sync ();
}

static void on_retry (GtkButton *b, gpointer d) { start_sync (); }

/* ---------- Developer helpers (SAWMILL_OPEN / SAWMILL_SCREENSHOT) ---------- */

void
store_window_debug_open (const char *what)
{
  StoreApp *a;
  const StoreCategory *c;

  if (!ui.catalog)
    return;
  if (g_str_has_prefix (what, "search:"))
    {
      gtk_search_bar_set_search_mode (GTK_SEARCH_BAR (ui.search_bar), TRUE);
      gtk_editable_set_text (GTK_EDITABLE (ui.search_entry), what + 7);
    }
  else if ((a = store_catalog_find (ui.catalog, what)))
    open_app (a);
  else if ((c = store_catalog_category (ui.catalog, what)))
    open_category (c);
  else
    adw_view_stack_set_visible_child_name (ui.views, what);
}

gboolean
store_window_debug_screenshot (const char *path)
{
  GdkPaintable *p = gtk_widget_paintable_new (ui.window);
  GtkSnapshot *snap = gtk_snapshot_new ();
  int w = gtk_widget_get_width (ui.window), h = gtk_widget_get_height (ui.window);
  GskRenderNode *node;
  gboolean ok = FALSE;

  gdk_paintable_snapshot (p, snap, w, h);
  node = gtk_snapshot_free_to_node (snap);
  if (node)
    {
      GdkTexture *tex = gsk_renderer_render_texture (gtk_native_get_renderer (GTK_NATIVE (ui.window)), node,
                                                     &GRAPHENE_RECT_INIT (0, 0, w, h));
      ok = gdk_texture_save_to_png (tex, path);
      g_object_unref (tex);
      gsk_render_node_unref (node);
    }
  g_object_unref (p);
  return ok;
}

/* ---------- Window ---------- */

static void
on_destroy (GtkWidget *w, gpointer data)
{
  g_clear_handle_id (&ui.carousel_timeout, g_source_remove);
  ui.window = NULL;
  ui.featured = NULL;
  ui.search_bar = NULL;
}

GtkWidget *
store_window_new (AdwApplication *app)
{
  GtkWidget *win = adw_application_window_new (GTK_APPLICATION (app));
  GtkWidget *views = adw_view_stack_new ();
  GtkWidget *content = gtk_stack_new ();
  GtkWidget *header = adw_header_bar_new ();
  GtkWidget *switcher = adw_view_switcher_new ();
  GtkWidget *switcher_bar = adw_view_switcher_bar_new ();
  GtkWidget *search_toggle = gtk_toggle_button_new ();
  GtkWidget *menu_btn = gtk_menu_button_new ();
  GtkWidget *search_clamp = adw_clamp_new ();
  GtkWidget *tv = adw_toolbar_view_new ();
  GtkWidget *nav = adw_navigation_view_new ();
  GtkWidget *toasts = adw_toast_overlay_new ();
  GtkWidget *loading = status_page ("folder-download-symbolic", "Getting the Catalog",
                                    "Downloading the list of apps. This only takes a moment.");
  GtkWidget *spinner = adw_spinner_new ();
  GtkWidget *retry = gtk_button_new_with_label ("Try Again");
  GtkWidget *installed, *updates, *explore, *search;
  GMenu *menu = g_menu_new ();
  AdwBreakpoint *bp;
  const char *env_dir = g_getenv ("SAWMILL_CATALOG_DIR");
  const char *env_url = g_getenv ("SAWMILL_CATALOG_URL");

  ui.window = win;
  ui.old_catalogs = g_ptr_array_new ();
  gtk_window_set_title (GTK_WINDOW (win), "Sawmill");
  gtk_window_set_default_size (GTK_WINDOW (win), 1040, 780);
  gtk_widget_set_size_request (win, 360, 294);

  ui.banner_css = gtk_css_provider_new ();
  gtk_style_context_add_provider_for_display (gdk_display_get_default (), GTK_STYLE_PROVIDER (ui.banner_css),
                                              GTK_STYLE_PROVIDER_PRIORITY_APPLICATION + 1);

  /* views */
  ui.views = ADW_VIEW_STACK (views);
  explore = page_scroller (&ui.explore_box, 32);
  adw_view_stack_add_titled_with_icon (ui.views, explore, "explore", "Explore", "starred-symbolic");

  ui.installed_stack = GTK_STACK (gtk_stack_new ());
  gtk_stack_add_named (ui.installed_stack,
                       status_page ("view-app-grid-symbolic", "No Apps Installed",
                                    "Apps you install from Sawmill appear here."), "empty");
  installed = page_scroller (&ui.installed_box, 12);
  gtk_stack_add_named (ui.installed_stack, installed, "list");
  adw_view_stack_add_titled_with_icon (ui.views, GTK_WIDGET (ui.installed_stack), "installed", "Installed",
                                       "view-app-grid-symbolic");

  ui.updates_stack = GTK_STACK (gtk_stack_new ());
  gtk_stack_add_named (ui.updates_stack,
                       status_page ("object-select-symbolic", "Up to Date", "Installed apps have no updates."),
                       "empty");
  updates = page_scroller (&ui.updates_box, 12);
  gtk_stack_add_named (ui.updates_stack, updates, "list");
  ui.updates_page = adw_view_stack_add_titled_with_icon (ui.views, GTK_WIDGET (ui.updates_stack), "updates",
                                                         "Updates", "software-update-available-symbolic");

  /* loading | error | browse | search */
  ui.content = GTK_STACK (content);
  gtk_stack_set_transition_type (ui.content, GTK_STACK_TRANSITION_TYPE_CROSSFADE);
  gtk_widget_set_size_request (spinner, 32, 32);
  adw_status_page_set_child (ADW_STATUS_PAGE (loading), spinner);
  gtk_stack_add_named (ui.content, loading, "loading");
  ui.error_page = ADW_STATUS_PAGE (status_page ("dialog-warning-symbolic", "", ""));
  gtk_widget_add_css_class (retry, "pill");
  gtk_widget_add_css_class (retry, "suggested-action");
  gtk_widget_set_halign (retry, GTK_ALIGN_CENTER);
  g_signal_connect (retry, "clicked", G_CALLBACK (on_retry), NULL);
  adw_status_page_set_child (ui.error_page, retry);
  gtk_stack_add_named (ui.content, GTK_WIDGET (ui.error_page), "error");
  gtk_stack_add_named (ui.content, views, "browse");
  search = page_scroller (&ui.search_box, 12);
  gtk_stack_add_named (ui.content, search, "search");
  g_signal_connect (views, "notify::visible-child", G_CALLBACK (on_view_changed), NULL);

  /* header bar */
  adw_view_switcher_set_stack (ADW_VIEW_SWITCHER (switcher), ui.views);
  adw_view_switcher_set_policy (ADW_VIEW_SWITCHER (switcher), ADW_VIEW_SWITCHER_POLICY_WIDE);
  adw_header_bar_set_title_widget (ADW_HEADER_BAR (header), switcher);
  gtk_button_set_icon_name (GTK_BUTTON (search_toggle), "system-search-symbolic");
  gtk_widget_set_tooltip_text (search_toggle, "Search");
  adw_header_bar_pack_start (ADW_HEADER_BAR (header), search_toggle);

  g_menu_append (menu, "_Refresh Catalog", "app.refresh");
  g_menu_append (menu, "_Dark Style", "app.dark");
  g_menu_append (menu, "_About Sawmill", "app.about");
  gtk_menu_button_set_icon_name (GTK_MENU_BUTTON (menu_btn), "open-menu-symbolic");
  gtk_menu_button_set_menu_model (GTK_MENU_BUTTON (menu_btn), G_MENU_MODEL (menu));
  gtk_menu_button_set_primary (GTK_MENU_BUTTON (menu_btn), TRUE);
  gtk_widget_set_tooltip_text (menu_btn, "Main Menu");
  adw_header_bar_pack_end (ADW_HEADER_BAR (header), menu_btn);
  g_object_unref (menu);

  /* search bar */
  ui.search_bar = gtk_search_bar_new ();
  ui.search_entry = gtk_search_entry_new ();
  gtk_search_entry_set_placeholder_text (GTK_SEARCH_ENTRY (ui.search_entry), "Search apps");
  adw_clamp_set_maximum_size (ADW_CLAMP (search_clamp), 420);
  adw_clamp_set_child (ADW_CLAMP (search_clamp), ui.search_entry);
  gtk_widget_set_hexpand (search_clamp, TRUE);
  gtk_search_bar_set_child (GTK_SEARCH_BAR (ui.search_bar), search_clamp);
  gtk_search_bar_connect_entry (GTK_SEARCH_BAR (ui.search_bar), GTK_EDITABLE (ui.search_entry));
  gtk_search_bar_set_key_capture_widget (GTK_SEARCH_BAR (ui.search_bar), win);
  g_object_bind_property (search_toggle, "active", ui.search_bar, "search-mode-enabled",
                          G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE);
  g_signal_connect (ui.search_entry, "search-changed", G_CALLBACK (on_search_changed), NULL);
  g_signal_connect (ui.search_bar, "notify::search-mode-enabled", G_CALLBACK (on_search_mode), NULL);

  adw_view_switcher_bar_set_stack (ADW_VIEW_SWITCHER_BAR (switcher_bar), ui.views);
  adw_toolbar_view_add_top_bar (ADW_TOOLBAR_VIEW (tv), header);
  adw_toolbar_view_add_top_bar (ADW_TOOLBAR_VIEW (tv), ui.search_bar);
  adw_toolbar_view_set_content (ADW_TOOLBAR_VIEW (tv), content);
  adw_toolbar_view_add_bottom_bar (ADW_TOOLBAR_VIEW (tv), switcher_bar);

  ui.nav = ADW_NAVIGATION_VIEW (nav);
  adw_navigation_view_add (ui.nav, adw_navigation_page_new_with_tag (tv, "Sawmill", "main"));
  ui.toasts = ADW_TOAST_OVERLAY (toasts);
  adw_toast_overlay_set_child (ui.toasts, nav);
  adw_application_window_set_content (ADW_APPLICATION_WINDOW (win), toasts);

  bp = adw_breakpoint_new (adw_breakpoint_condition_parse ("max-width: 600sp"));
  adw_breakpoint_add_setters (bp, G_OBJECT (switcher_bar), "reveal", TRUE, G_OBJECT (switcher), "visible", FALSE, NULL);
  adw_application_window_add_breakpoint (ADW_APPLICATION_WINDOW (win), bp);

  /* catalog: a local folder (development), or the git clone in ~/.cache/sawmill */
  if (env_dir && *env_dir)
    ui.catalog_dir = g_canonicalize_filename (env_dir, NULL);
  else
    {
      ui.clone_dir = g_build_filename (g_get_user_cache_dir (), "sawmill", NULL);
      ui.catalog_dir = g_build_filename (ui.clone_dir, "catalog", NULL);
      ui.repo_url = g_strdup (env_url && *env_url ? env_url : SAWMILL_CATALOG_URL);
    }

  {
    g_autoptr (GError) error = NULL;
    if (!load_catalog (&error))
      {
        if (!ui.clone_dir || !g_error_matches (error, STORE_CATALOG_ERROR, STORE_CATALOG_ERROR_MISSING))
          show_error (g_error_matches (error, STORE_CATALOG_ERROR, STORE_CATALOG_ERROR_TOO_NEW)
                      ? "Update Sawmill" : "The Catalog Could Not Be Read", error->message);
        else
          gtk_stack_set_visible_child_name (ui.content, "loading");
      }
  }
  start_sync ();      /* refresh in the background; no-op for a local folder */

  ui.carousel_timeout = g_timeout_add_seconds (7, auto_advance, NULL);
  g_signal_connect (win, "destroy", G_CALLBACK (on_destroy), NULL);
  return win;
}
