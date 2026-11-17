/* Main window: Explore / Installed / Updates, app details, categories and search. */

#include <math.h>
#include "store-window.h"
#include "store-data.h"
#include "store-art.h"

/* The mockup has a single window, so its widgets live in one static struct. */
typedef struct {
  GtkWidget             *window;
  AdwNavigationView     *nav;
  AdwToastOverlay       *toasts;
  AdwViewStackPage      *updates_page;
  GtkStack              *content;
  GtkWidget             *search_bar;
  GtkWidget             *search_entry;
  GtkWidget             *search_box;
  GtkWidget             *installed_box;
  GtkStack              *updates_stack;
  GtkWidget             *updates_box;
  AdwCarousel           *featured;
  GtkEventController    *featured_motion;
  guint                  carousel_timeout;
} Ui;

static Ui ui;

static void open_app      (StoreApp *a);
static void refresh_lists (void);

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

static char *
stars_text (double rating)
{
  int n = (int) round (rating);
  GString *s = g_string_new (NULL);
  for (int i = 0; i < 5; i++)
    g_string_append (s, i < n ? "★" : "☆");
  return g_string_free (s, FALSE);
}

static char *
thousands (int n)
{
  if (n >= 1000)
    return g_strdup_printf ("%d,%03d", n / 1000, n % 1000);
  return g_strdup_printf ("%d", n);
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

/* ---------- per-app watchers: widgets that redraw when an app's state changes ---------- */

typedef void (*WatchFn) (GtkWidget *w, StoreApp *a);

typedef struct {
  GtkWidget *widget;
  WatchFn    fn;
  StoreApp  *app;
} Watch;

static void
watch_destroyed (GtkWidget *w, Watch *wt)
{
  g_ptr_array_remove (wt->app->watchers, wt);
}

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

/* ---------- install / update jobs (simulated) ---------- */

static gboolean
job_tick (gpointer data)
{
  StoreApp *a = data;
  double step = 1.0 / (8 + a->size_mb / 12);

  a->progress = MIN (1.0, a->progress + step * (0.6 + g_random_double () * 0.8));
  if (a->progress >= 1.0)
    {
      gboolean was_update = a->busy_is_update;
      g_autofree char *msg = NULL;

      a->busy = FALSE;
      a->state = APP_INSTALLED;
      notify_app (a);
      refresh_lists ();
      msg = was_update ? g_strdup_printf ("%s was updated", a->name)
                       : g_strdup_printf ("%s is installed", a->name);
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
  g_autofree char *msg = g_strdup_printf ("%s was uninstalled", a->name);
  a->state = APP_AVAILABLE;
  notify_app (a);
  refresh_lists ();
  toast (msg);
}

static void
on_open (GtkButton *b, StoreApp *a)
{
  g_autofree char *msg = g_strdup_printf ("Launching %s is not part of this mockup", a->name);
  toast (msg);
}

static void
on_update_all (GtkButton *b, gpointer data)
{
  for (guint i = 0; i < store_n_apps; i++)
    if (store_apps[i].state == APP_UPDATE)
      start_job (&store_apps[i], TRUE);
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
  GtkWidget *name, *sum, *pill;
  g_autofree char *stars = stars_text (a->rating);

  gtk_widget_add_css_class (btn, "card");
  gtk_widget_add_css_class (btn, "tile");

  gtk_box_append (GTK_BOX (hbox), store_app_icon_new (a, 64));

  name = label_new (a->name, "heading", FALSE);
  gtk_label_set_ellipsize (GTK_LABEL (name), PANGO_ELLIPSIZE_END);
  sum = label_new (a->summary, "dim-label", TRUE);
  gtk_label_set_lines (GTK_LABEL (sum), 2);
  gtk_label_set_ellipsize (GTK_LABEL (sum), PANGO_ELLIPSIZE_END);
  gtk_label_set_width_chars (GTK_LABEL (sum), 18);
  gtk_label_set_max_width_chars (GTK_LABEL (sum), 30);

  gtk_box_append (GTK_BOX (meta), label_new (stars, "stars caption", FALSE));
  pill = label_new ("", "pill caption-heading", FALSE);
  watch (a, pill, update_pill);
  gtk_box_append (GTK_BOX (meta), pill);
  gtk_widget_set_margin_top (meta, 4);

  gtk_box_append (GTK_BOX (vbox), name);
  gtk_box_append (GTK_BOX (vbox), sum);
  gtk_box_append (GTK_BOX (vbox), meta);
  gtk_widget_set_valign (vbox, GTK_ALIGN_CENTER);
  gtk_widget_set_hexpand (vbox, TRUE);
  gtk_box_append (GTK_BOX (hbox), vbox);

  gtk_button_set_child (GTK_BUTTON (btn), hbox);
  gtk_accessible_update_property (GTK_ACCESSIBLE (btn), GTK_ACCESSIBLE_PROPERTY_LABEL, a->name, -1);
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
tiles_for (const char **ids, guint n)
{
  GtkWidget *fb = flow_new (3);
  for (guint i = 0; i < n; i++)
    flow_add (fb, tile_new (store_app_by_id (ids[i])));
  return fb;
}

/* ---------- carousels ---------- */

static void
carousel_step (AdwCarousel *c, int delta)
{
  int n = (int) adw_carousel_get_n_pages (c);
  int pos = (int) round (adw_carousel_get_position (c));
  int next = ((pos + delta) % n + n) % n;
  adw_carousel_scroll_to (c, adw_carousel_get_nth_page (c, next), TRUE);
}

static void on_prev (GtkButton *b, AdwCarousel *c) { carousel_step (c, -1); }
static void on_next (GtkButton *b, AdwCarousel *c) { carousel_step (c, 1); }

static GtkWidget *
carousel_frame (AdwCarousel *c, GtkEventController **motion_out)
{
  GtkWidget *box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 8);
  GtkWidget *overlay = gtk_overlay_new ();
  GtkWidget *prev = gtk_button_new_from_icon_name ("go-previous-symbolic");
  GtkWidget *next = gtk_button_new_from_icon_name ("go-next-symbolic");
  GtkWidget *dots = adw_carousel_indicator_dots_new ();

  gtk_overlay_set_child (GTK_OVERLAY (overlay), GTK_WIDGET (c));
  for (int i = 0; i < 2; i++)
    {
      GtkWidget *b = i ? next : prev;
      gtk_widget_add_css_class (b, "circular");
      gtk_widget_add_css_class (b, "osd");
      gtk_widget_set_valign (b, GTK_ALIGN_CENTER);
      gtk_widget_set_halign (b, i ? GTK_ALIGN_END : GTK_ALIGN_START);
      gtk_widget_set_margin_start (b, 12);
      gtk_widget_set_margin_end (b, 12);
      gtk_widget_set_tooltip_text (b, i ? "Next" : "Previous");
      gtk_overlay_add_overlay (GTK_OVERLAY (overlay), b);
    }
  g_signal_connect (prev, "clicked", G_CALLBACK (on_prev), c);
  g_signal_connect (next, "clicked", G_CALLBACK (on_next), c);

  adw_carousel_indicator_dots_set_carousel (ADW_CAROUSEL_INDICATOR_DOTS (dots), c);
  gtk_box_append (GTK_BOX (box), overlay);
  gtk_box_append (GTK_BOX (box), dots);

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
  if (ui.featured && gtk_widget_get_mapped (GTK_WIDGET (ui.featured)) &&
      !gtk_event_controller_motion_contains_pointer (GTK_EVENT_CONTROLLER_MOTION (ui.featured_motion)))
    carousel_step (ui.featured, 1);
  return G_SOURCE_CONTINUE;
}

/* ---------- Explore ---------- */

static GtkWidget *
banner_new (StoreApp *a)
{
  GtkWidget *btn = gtk_button_new ();
  GtkWidget *hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 24);
  GtkWidget *text = gtk_box_new (GTK_ORIENTATION_VERTICAL, 10);
  GtkWidget *icon = store_app_icon_new (a, 72);
  GtkWidget *shot = store_screenshot_new (a, a->shots[0], 230, 0.0, 1.0);
  g_autofree char *cls = g_strdup_printf ("app-bg-%s", a->id);

  gtk_widget_add_css_class (btn, "banner");
  gtk_widget_add_css_class (btn, cls);
  gtk_widget_set_overflow (btn, GTK_OVERFLOW_HIDDEN);
  gtk_widget_set_hexpand (btn, TRUE);
  gtk_widget_set_size_request (btn, -1, 280);

  gtk_box_set_homogeneous (GTK_BOX (hbox), TRUE);
  gtk_widget_set_halign (icon, GTK_ALIGN_START);
  gtk_box_append (GTK_BOX (text), icon);
  gtk_box_append (GTK_BOX (text), label_new (a->name, "banner-title", TRUE));
  gtk_box_append (GTK_BOX (text), label_new (a->summary, "banner-summary", TRUE));
  gtk_widget_set_valign (text, GTK_ALIGN_CENTER);
  gtk_widget_set_margin_start (text, 36);
  gtk_widget_set_margin_top (text, 28);
  gtk_widget_set_margin_bottom (text, 28);

  gtk_widget_set_valign (shot, GTK_ALIGN_END);
  gtk_widget_set_margin_top (shot, 36);

  gtk_box_append (GTK_BOX (hbox), text);
  gtk_box_append (GTK_BOX (hbox), shot);
  gtk_button_set_child (GTK_BUTTON (btn), hbox);
  gtk_accessible_update_property (GTK_ACCESSIBLE (btn), GTK_ACCESSIBLE_PROPERTY_LABEL, a->name, -1);
  g_signal_connect (btn, "clicked", G_CALLBACK (on_tile_clicked), a);
  return btn;
}

static void open_category (const StoreCategory *c);
static void on_category_clicked (GtkButton *b, StoreCategory *c) { open_category (c); }

static GtkWidget *
build_explore (void)
{
  GtkWidget *box, *sw = page_scroller (&box, 32);
  AdwCarousel *c = ADW_CAROUSEL (adw_carousel_new ());
  GtkWidget *cats = flow_new (4);
  GtkWidget *note;

  adw_carousel_set_spacing (c, 12);
  for (guint i = 0; i < store_n_featured; i++)
    adw_carousel_append (c, banner_new (store_app_by_id (store_featured[i])));
  ui.featured = c;
  gtk_box_append (GTK_BOX (box), carousel_frame (c, &ui.featured_motion));

  gtk_box_append (GTK_BOX (box), section ("Editor's Choice", tiles_for (store_picks, store_n_picks)));

  for (guint i = 0; i < store_n_categories; i++)
    {
      StoreCategory *cat = &store_categories[i];
      GtkWidget *btn = gtk_button_new ();
      GtkWidget *hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 12);
      GtkWidget *glyph = store_glyph_icon_new (cat->glyph, 22);

      gtk_widget_add_css_class (btn, "card");
      gtk_widget_add_css_class (btn, "category");
      gtk_widget_add_css_class (glyph, "accent");
      gtk_box_append (GTK_BOX (hbox), glyph);
      gtk_box_append (GTK_BOX (hbox), label_new (cat->name, "heading", FALSE));
      gtk_button_set_child (GTK_BUTTON (btn), hbox);
      g_signal_connect (btn, "clicked", G_CALLBACK (on_category_clicked), cat);
      flow_add (cats, btn);
    }
  gtk_box_append (GTK_BOX (box), section ("Categories", cats));

  gtk_box_append (GTK_BOX (box), section ("New & Updated", tiles_for (store_new, store_n_new)));

  note = label_new ("Mockup with fictional apps and generated artwork.", "dim-label caption", FALSE);
  gtk_label_set_xalign (GTK_LABEL (note), 0.5);
  gtk_box_append (GTK_BOX (box), note);
  return sw;
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
  g_autofree char *sub = NULL;

  adw_preferences_row_set_use_markup (ADW_PREFERENCES_ROW (row), FALSE);
  adw_preferences_row_set_title (ADW_PREFERENCES_ROW (row), a->name);
  if (a->busy)
    sub = g_strdup (a->busy_is_update ? "Updating…" : "Installing…");
  else if (for_updates)
    sub = g_strdup_printf ("Installed version %s · %.1f MB to download", a->version, a->size_mb * 0.4);
  else
    sub = g_strdup_printf ("Version %s · %.1f MB · %s", a->version, a->size_mb, a->developer);
  adw_action_row_set_subtitle (ADW_ACTION_ROW (row), sub);
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
refresh_installed (void)
{
  GtkWidget *head = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 12);
  GtkWidget *list = boxed_list_new ();
  GtkWidget *title;
  guint n = 0;
  double disk = 0;
  g_autofree char *t = NULL, *d = NULL;

  clear_box (ui.installed_box);
  for (guint i = 0; i < store_n_apps; i++)
    {
      StoreApp *a = &store_apps[i];
      if (a->state == APP_AVAILABLE && !a->busy)
        continue;
      n++;
      disk += a->size_mb * 2.6;
      gtk_list_box_append (GTK_LIST_BOX (list), app_row_new (a, FALSE));
    }

  t = g_strdup_printf ("%u Installed Apps", n);
  d = g_strdup_printf ("%.2f GB on disk", disk / 1024);
  title = label_new (t, "title-4", FALSE);
  gtk_widget_set_hexpand (title, TRUE);
  gtk_box_append (GTK_BOX (head), title);
  gtk_box_append (GTK_BOX (head), label_new (d, "dim-label caption", FALSE));
  gtk_box_append (GTK_BOX (ui.installed_box), head);
  gtk_box_append (GTK_BOX (ui.installed_box), list);
}

static void
refresh_updates (void)
{
  guint n = 0;
  double dl = 0;

  for (guint i = 0; i < store_n_apps; i++)
    if (store_apps[i].state == APP_UPDATE)
      {
        n++;
        dl += store_apps[i].size_mb * 0.4;
      }

  adw_view_stack_page_set_badge_number (ui.updates_page, n);
  adw_view_stack_page_set_needs_attention (ui.updates_page, n > 0);

  if (n == 0)
    {
      gtk_stack_set_visible_child_name (ui.updates_stack, "empty");
      return;
    }

  {
    GtkWidget *head = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 12);
    GtkWidget *btn = gtk_button_new_with_label ("Update All");
    GtkWidget *list = boxed_list_new ();
    GtkWidget *title;
    g_autofree char *t = g_strdup_printf ("%u Updates · %.1f MB to download", n, dl);

    clear_box (ui.updates_box);
    title = label_new (t, "title-4", TRUE);
    gtk_widget_set_hexpand (title, TRUE);
    gtk_widget_add_css_class (btn, "suggested-action");
    gtk_widget_set_valign (btn, GTK_ALIGN_CENTER);
    g_signal_connect (btn, "clicked", G_CALLBACK (on_update_all), NULL);
    gtk_box_append (GTK_BOX (head), title);
    gtk_box_append (GTK_BOX (head), btn);

    for (guint i = 0; i < store_n_apps; i++)
      if (store_apps[i].state == APP_UPDATE)
        gtk_list_box_append (GTK_LIST_BOX (list), app_row_new (&store_apps[i], TRUE));

    gtk_box_append (GTK_BOX (ui.updates_box), head);
    gtk_box_append (GTK_BOX (ui.updates_box), list);
    gtk_box_append (GTK_BOX (ui.updates_box),
                    label_new ("Updates include bug fixes, translations and performance improvements. "
                               "Apps restart with the new version the next time you open them.",
                               "dim-label", TRUE));
    gtk_stack_set_visible_child_name (ui.updates_stack, "list");
  }
}

static void
refresh_lists (void)
{
  refresh_installed ();
  refresh_updates ();
}

static void
on_check_updates (GtkButton *b, gpointer data)
{
  toast ("Checked for updates. Everything is up to date.");
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

static GtkWidget *
build_reviews (StoreApp *a)
{
  static const double dist[] = { .62, .24, .08, .04, .02 };
  static const char *months[] = { "October", "September", "August" };
  GtkWidget *box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 12);
  GtkWidget *score = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 32);
  GtkWidget *left = gtk_box_new (GTK_ORIENTATION_VERTICAL, 2);
  GtkWidget *grid = gtk_grid_new ();
  GtkWidget *list = boxed_list_new ();
  int seed = (int) strlen (a->name);
  g_autofree char *big = g_strdup_printf ("%.1f", a->rating);
  g_autofree char *stars = stars_text (a->rating);
  g_autofree char *votes = thousands (a->votes);
  g_autofree char *votes_text = g_strdup_printf ("%s ratings", votes);

  gtk_widget_add_css_class (score, "card");
  gtk_widget_add_css_class (score, "score-card");
  gtk_widget_set_valign (left, GTK_ALIGN_CENTER);
  gtk_box_append (GTK_BOX (left), label_new (big, "score-big", FALSE));
  gtk_box_append (GTK_BOX (left), label_new (stars, "stars", FALSE));
  gtk_box_append (GTK_BOX (left), label_new (votes_text, "dim-label caption", FALSE));
  gtk_box_append (GTK_BOX (score), left);

  gtk_grid_set_row_spacing (GTK_GRID (grid), 4);
  gtk_grid_set_column_spacing (GTK_GRID (grid), 10);
  gtk_widget_set_hexpand (grid, TRUE);
  gtk_widget_set_valign (grid, GTK_ALIGN_CENTER);
  for (int k = 0; k < 5; k++)
    {
      g_autofree char *n = g_strdup_printf ("%d", 5 - k);
      GtkWidget *pb = gtk_progress_bar_new ();
      gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (pb), dist[k]);
      gtk_widget_add_css_class (pb, "rating-bar");
      gtk_widget_set_hexpand (pb, TRUE);
      gtk_widget_set_valign (pb, GTK_ALIGN_CENTER);
      gtk_grid_attach (GTK_GRID (grid), label_new (n, "dim-label caption numeric", FALSE), 0, k, 1, 1);
      gtk_grid_attach (GTK_GRID (grid), pb, 1, k, 1, 1);
    }
  gtk_box_append (GTK_BOX (score), grid);

  for (int k = 0; k < 3; k++)
    {
      const StoreReview *r = &store_reviews[(seed + k) % store_n_reviews];
      const char *who = store_reviewers[(seed + k * 3) % store_n_reviewers];
      GtkWidget *row = gtk_list_box_row_new ();
      GtkWidget *v = gtk_box_new (GTK_ORIENTATION_VERTICAL, 4);
      GtkWidget *head = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 8);
      GtkWidget *name;
      g_autofree char *rs = stars_text (r->stars);
      g_autofree char *date = g_strdup_printf ("%s %d, 2026", months[k], 3 + (seed * 7 + k * 5) % 25);

      gtk_widget_add_css_class (v, "review-row");
      gtk_box_append (GTK_BOX (head), label_new (rs, "stars caption", FALSE));
      name = label_new (who, "caption-heading", FALSE);
      gtk_widget_set_hexpand (name, TRUE);
      gtk_box_append (GTK_BOX (head), name);
      gtk_box_append (GTK_BOX (head), label_new (date, "dim-label caption", FALSE));
      gtk_box_append (GTK_BOX (v), head);
      gtk_box_append (GTK_BOX (v), label_new (r->title, "heading", TRUE));
      gtk_box_append (GTK_BOX (v), label_new (r->body, NULL, TRUE));
      gtk_list_box_row_set_child (GTK_LIST_BOX_ROW (row), v);
      gtk_list_box_row_set_activatable (GTK_LIST_BOX_ROW (row), FALSE);
      gtk_list_box_append (GTK_LIST_BOX (list), row);
    }

  gtk_box_append (GTK_BOX (box), score);
  gtk_box_append (GTK_BOX (box), list);
  return box;
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
  GtkWidget *box, *sw = page_scroller (&box, 32);
  GtkWidget *hero = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 24);
  GtkWidget *info = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
  GtkWidget *dev = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
  GtkWidget *rating = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
  GtkWidget *actions = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
  GtkWidget *action = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 8);
  GtkWidget *source, *shots_box, *desc, *ctx, *more;
  AdwCarousel *shots = ADW_CAROUSEL (adw_carousel_new ());
  const StoreCategory *cat = store_category_by_id (a->category);
  g_autofree char *stars = stars_text (a->rating);
  g_autofree char *votes = thousands (a->votes);
  g_autofree char *rating_text = g_strdup_printf ("%.1f · %s ratings", a->rating, votes);
  g_autofree char *cat_text = g_strdup_printf ("· %s", cat->name);
  g_autofree char *src = g_strdup_printf ("Flatpak · flathub · v%s", a->version);
  g_autofree char *size = a->size_mb >= 100 ? g_strdup_printf ("%.0f MB", a->size_mb) : g_strdup_printf ("%.1f MB", a->size_mb);
  g_autofree char *disk = g_strdup_printf ("Needs %.0f MB on disk", a->size_mb * 2.6);
  g_autofree char *more_title = g_strdup_printf ("More in %s", cat->name);
  guint n_more = 0;

  /* hero */
  gtk_box_append (GTK_BOX (hero), store_app_icon_new (a, 128));
  gtk_box_append (GTK_BOX (info), label_new (a->name, "title-1", TRUE));
  gtk_box_append (GTK_BOX (dev), label_new (a->developer, "dim-label", FALSE));
  if (a->verified)
    {
      GtkWidget *v = gtk_image_new_from_icon_name ("object-select-symbolic");
      gtk_widget_add_css_class (v, "accent");
      gtk_widget_set_tooltip_text (v, "Verified developer");
      gtk_box_append (GTK_BOX (dev), v);
    }
  gtk_box_append (GTK_BOX (dev), label_new (cat_text, "dim-label", FALSE));
  gtk_box_append (GTK_BOX (info), dev);
  gtk_box_append (GTK_BOX (rating), label_new (stars, "stars", FALSE));
  gtk_box_append (GTK_BOX (rating), label_new (rating_text, "dim-label", FALSE));
  gtk_box_append (GTK_BOX (info), rating);
  gtk_widget_set_valign (info, GTK_ALIGN_CENTER);
  gtk_widget_set_hexpand (info, TRUE);
  gtk_box_append (GTK_BOX (hero), info);

  gtk_widget_set_halign (action, GTK_ALIGN_CENTER);
  watch (a, action, update_action);
  source = label_new (src, "dim-label caption", FALSE);
  gtk_label_set_xalign (GTK_LABEL (source), 0.5);
  gtk_box_append (GTK_BOX (actions), action);
  gtk_box_append (GTK_BOX (actions), source);
  gtk_widget_set_valign (actions, GTK_ALIGN_CENTER);
  gtk_box_append (GTK_BOX (hero), actions);
  gtk_box_append (GTK_BOX (box), hero);

  /* screenshots */
  adw_carousel_set_spacing (shots, 24);
  for (int i = 0; i < STORE_N_SHOTS; i++)
    adw_carousel_append (shots, store_screenshot_new (a, a->shots[i], 380, 0.5, 0.5));
  shots_box = carousel_frame (shots, NULL);
  gtk_widget_add_css_class (shots_box, "shots-frame");
  gtk_box_append (GTK_BOX (box), shots_box);

  /* description */
  desc = gtk_box_new (GTK_ORIENTATION_VERTICAL, 10);
  gtk_box_append (GTK_BOX (desc), label_new (a->summary, "title-2", TRUE));
  gtk_box_append (GTK_BOX (desc), label_new (a->description, NULL, TRUE));
  for (int i = 0; i < 4; i++)
    {
      g_autofree char *f = g_strdup_printf ("•  %s", a->features[i]);
      gtk_box_append (GTK_BOX (desc), label_new (f, NULL, TRUE));
    }
  gtk_box_append (GTK_BOX (box), desc);

  /* context tiles */
  ctx = flow_new (4);
  gtk_flow_box_set_min_children_per_line (GTK_FLOW_BOX (ctx), 2);
  flow_add (ctx, context_tile (gtk_label_new (size), "lz-neutral", "Download Size", disk));
  flow_add (ctx, context_tile (icon_image (a->safe ? "security-high-symbolic" : "security-low-symbolic"),
                               a->safe ? "lz-ok" : "lz-warn",
                               a->safe ? "Safe" : "Potentially Unsafe",
                               a->safe ? "Runs sandboxed with no network access" : "Has network and file system access"));
  flow_add (ctx, context_tile (icon_image (a->adaptive ? "phone-symbolic" : "computer-symbolic"), "lz-blue",
                               a->adaptive ? "Adaptive" : "Desktop Only",
                               a->adaptive ? "Works on phones, tablets and desktops" : "Needs a large screen, keyboard and mouse"));
  flow_add (ctx, context_tile (gtk_label_new (g_str_equal (a->category, "play") ? "7+" : "All"), "lz-neutral",
                               "Age Rating", a->free_software ? "Free software license" : "Proprietary license"));
  gtk_box_append (GTK_BOX (box), ctx);

  gtk_box_append (GTK_BOX (box), section ("Reviews & Ratings", build_reviews (a)));

  more = flow_new (3);
  for (guint i = 0; i < store_n_apps; i++)
    if (&store_apps[i] != a && g_str_equal (store_apps[i].category, a->category))
      {
        flow_add (more, tile_new (&store_apps[i]));
        n_more++;
      }
  if (n_more)
    gtk_box_append (GTK_BOX (box), section (more_title, more));
  else
    g_object_ref_sink (more), g_object_unref (more);

  push_page (a->name, sw);
}

static void
open_category (const StoreCategory *c)
{
  GtkWidget *box, *sw = page_scroller (&box, 24);
  GtkWidget *fb = flow_new (3);
  GtkWidget *title = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 12);
  GtkWidget *glyph = store_glyph_icon_new (c->glyph, 32);
  guint n = 0;
  g_autofree char *count = NULL;

  for (guint i = 0; i < store_n_apps; i++)
    if (g_str_equal (store_apps[i].category, c->id))
      {
        flow_add (fb, tile_new (&store_apps[i]));
        n++;
      }
  count = g_strdup_printf ("%u apps", n);
  gtk_widget_add_css_class (glyph, "accent");
  gtk_box_append (GTK_BOX (title), glyph);
  gtk_box_append (GTK_BOX (title), label_new (c->name, "title-1", FALSE));
  gtk_box_append (GTK_BOX (box), title);
  gtk_box_append (GTK_BOX (box), section (count, fb));
  push_page (c->name, sw);
}

/* ---------- Search ---------- */

static void
on_search_changed (GtkSearchEntry *entry, gpointer data)
{
  const char *q = gtk_editable_get_text (GTK_EDITABLE (entry));
  g_autofree char *needle = NULL;
  GtkWidget *fb;
  guint n = 0;

  if (!q || !*q)
    {
      gtk_stack_set_visible_child_name (ui.content, "browse");
      return;
    }

  clear_box (ui.search_box);
  needle = g_utf8_casefold (q, -1);
  fb = flow_new (3);
  for (guint i = 0; i < store_n_apps; i++)
    {
      StoreApp *a = &store_apps[i];
      g_autofree char *hay = g_strdup_printf ("%s %s %s %s", a->name, a->summary, a->developer,
                                              store_category_by_id (a->category)->name);
      g_autofree char *fold = g_utf8_casefold (hay, -1);
      if (strstr (fold, needle))
        {
          flow_add (fb, tile_new (a));
          n++;
        }
    }

  if (n == 0)
    {
      GtkWidget *status = adw_status_page_new ();
      adw_status_page_set_icon_name (ADW_STATUS_PAGE (status), "system-search-symbolic");
      adw_status_page_set_title (ADW_STATUS_PAGE (status), "No Results Found");
      adw_status_page_set_description (ADW_STATUS_PAGE (status), "Try a different name, or a word like “music”, “notes” or “game”.");
      gtk_widget_set_vexpand (status, TRUE);
      gtk_box_append (GTK_BOX (ui.search_box), status);
      g_object_ref_sink (fb);
      g_object_unref (fb);
    }
  else
    {
      g_autofree char *t = g_strdup_printf ("%u result%s", n, n == 1 ? "" : "s");
      gtk_box_append (GTK_BOX (ui.search_box), section (t, fb));
    }
  gtk_stack_set_visible_child_name (ui.content, "search");
}

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

/* ---------- Developer helpers (SAWMILL_OPEN / SAWMILL_SCREENSHOT) ---------- */

void
store_window_debug_open (const char *what)
{
  StoreApp *a;
  const StoreCategory *c;

  if (g_str_equal (what, "search"))
    {
      gtk_search_bar_set_search_mode (GTK_SEARCH_BAR (ui.search_bar), TRUE);
      gtk_editable_set_text (GTK_EDITABLE (ui.search_entry), "music");
    }
  else if ((a = store_app_by_id (what)))
    open_app (a);
  else if ((c = store_category_by_id (what)))
    open_category (c);
  else
    {
      GtkWidget *stack = gtk_stack_get_child_by_name (ui.content, "browse");
      adw_view_stack_set_visible_child_name (ADW_VIEW_STACK (stack), what);
    }
}

gboolean
store_window_debug_screenshot (const char *path)
{
  GdkPaintable *p = gtk_widget_paintable_new (ui.window);
  GtkSnapshot *snap = gtk_snapshot_new ();
  int w = gtk_widget_get_width (ui.window), h = gtk_widget_get_height (ui.window);
  GskRenderNode *node;
  GdkTexture *tex;
  gboolean ok = FALSE;

  gdk_paintable_snapshot (p, snap, w, h);
  node = gtk_snapshot_free_to_node (snap);
  if (node)
    {
      tex = gsk_renderer_render_texture (gtk_native_get_renderer (GTK_NATIVE (ui.window)), node,
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
  ui.featured = NULL;
  ui.search_bar = NULL;
}

GtkWidget *
store_window_new (AdwApplication *app)
{
  GtkWidget *win = adw_application_window_new (GTK_APPLICATION (app));
  GtkWidget *stack = adw_view_stack_new ();
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
  GtkWidget *empty = adw_status_page_new ();
  GtkWidget *check = gtk_button_new_with_label ("Check for Updates");
  GtkWidget *installed_sw, *updates_sw, *search_sw;
  GMenu *menu = g_menu_new ();
  AdwBreakpoint *bp;

  ui.window = win;
  gtk_window_set_title (GTK_WINDOW (win), "Sawmill");
  gtk_window_set_default_size (GTK_WINDOW (win), 1040, 780);
  gtk_widget_set_size_request (win, 360, 294);

  /* pages */
  adw_view_stack_add_titled_with_icon (ADW_VIEW_STACK (stack), build_explore (), "explore", "Explore", "starred-symbolic");

  installed_sw = page_scroller (&ui.installed_box, 12);
  adw_view_stack_add_titled_with_icon (ADW_VIEW_STACK (stack), installed_sw, "installed", "Installed", "view-app-grid-symbolic");

  ui.updates_stack = GTK_STACK (gtk_stack_new ());
  adw_status_page_set_icon_name (ADW_STATUS_PAGE (empty), "object-select-symbolic");
  adw_status_page_set_title (ADW_STATUS_PAGE (empty), "Up to Date");
  adw_status_page_set_description (ADW_STATUS_PAGE (empty), "Last checked a moment ago.");
  gtk_widget_add_css_class (check, "pill");
  gtk_widget_set_halign (check, GTK_ALIGN_CENTER);
  g_signal_connect (check, "clicked", G_CALLBACK (on_check_updates), NULL);
  adw_status_page_set_child (ADW_STATUS_PAGE (empty), check);
  gtk_stack_add_named (ui.updates_stack, empty, "empty");
  updates_sw = page_scroller (&ui.updates_box, 12);
  gtk_stack_add_named (ui.updates_stack, updates_sw, "list");
  ui.updates_page = adw_view_stack_add_titled_with_icon (ADW_VIEW_STACK (stack), GTK_WIDGET (ui.updates_stack),
                                                         "updates", "Updates", "software-update-available-symbolic");

  /* browse vs. search results */
  ui.content = GTK_STACK (content);
  gtk_stack_set_transition_type (ui.content, GTK_STACK_TRANSITION_TYPE_CROSSFADE);
  gtk_stack_add_named (ui.content, stack, "browse");
  search_sw = page_scroller (&ui.search_box, 12);
  gtk_stack_add_named (ui.content, search_sw, "search");
  g_signal_connect (stack, "notify::visible-child", G_CALLBACK (on_view_changed), NULL);

  /* header bar */
  adw_view_switcher_set_stack (ADW_VIEW_SWITCHER (switcher), ADW_VIEW_STACK (stack));
  adw_view_switcher_set_policy (ADW_VIEW_SWITCHER (switcher), ADW_VIEW_SWITCHER_POLICY_WIDE);
  adw_header_bar_set_title_widget (ADW_HEADER_BAR (header), switcher);

  gtk_button_set_icon_name (GTK_BUTTON (search_toggle), "system-search-symbolic");
  gtk_widget_set_tooltip_text (search_toggle, "Search");
  adw_header_bar_pack_start (ADW_HEADER_BAR (header), search_toggle);

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

  adw_view_switcher_bar_set_stack (ADW_VIEW_SWITCHER_BAR (switcher_bar), ADW_VIEW_STACK (stack));

  adw_toolbar_view_add_top_bar (ADW_TOOLBAR_VIEW (tv), header);
  adw_toolbar_view_add_top_bar (ADW_TOOLBAR_VIEW (tv), ui.search_bar);
  adw_toolbar_view_set_content (ADW_TOOLBAR_VIEW (tv), content);
  adw_toolbar_view_add_bottom_bar (ADW_TOOLBAR_VIEW (tv), switcher_bar);

  ui.nav = ADW_NAVIGATION_VIEW (nav);
  adw_navigation_view_add (ui.nav, adw_navigation_page_new_with_tag (tv, "Sawmill", "main"));
  ui.toasts = ADW_TOAST_OVERLAY (toasts);
  adw_toast_overlay_set_child (ui.toasts, nav);
  adw_application_window_set_content (ADW_APPLICATION_WINDOW (win), toasts);

  /* narrow windows: move the view switcher to the bottom */
  bp = adw_breakpoint_new (adw_breakpoint_condition_parse ("max-width: 600sp"));
  adw_breakpoint_add_setters (bp,
                              G_OBJECT (switcher_bar), "reveal", TRUE,
                              G_OBJECT (switcher), "visible", FALSE,
                              NULL);
  adw_application_window_add_breakpoint (ADW_APPLICATION_WINDOW (win), bp);

  refresh_lists ();
  ui.carousel_timeout = g_timeout_add_seconds (7, auto_advance, NULL);
  g_signal_connect (win, "destroy", G_CALLBACK (on_destroy), NULL);
  return win;
}
