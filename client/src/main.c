/* Sawmill: a GNOME Software-style app store mockup (GTK 4 + libadwaita). */

#include <adwaita.h>
#include "store-window.h"

#define APP_ID "io.sawmill.Store"

static const char *base_css =
  ".tile { padding: 14px; }\n"
  ".category { padding: 18px 16px; }\n"
  "button.banner { padding: 0; border-radius: 12px; color: white; box-shadow: none; background-color: #3584e4; }\n"
  "button.banner:hover { filter: brightness(1.06); }\n"
  ".banner-title { font-size: 24pt; font-weight: 800; letter-spacing: -0.02em; }\n"
  ".banner-summary { font-size: 13pt; opacity: 0.92; }\n"
  ".banner-developer { opacity: 0.75; }\n"
  ".banner-shot { border-radius: 8px; }\n"
  ".stars { color: #e5a50a; }\n"
  ".pill.caption-heading { border-radius: 999px; padding: 1px 8px; }\n"
  ".pill-ok { color: var(--success-color); background-color: alpha(var(--success-color), 0.15); }\n"
  ".pill-up { color: var(--accent-color); background-color: alpha(var(--accent-color), 0.15); }\n"
  ".shots-frame { background-color: alpha(currentColor, 0.06); border-radius: 12px; padding: 24px 12px 12px; }\n"
  ".context-tile { padding: 18px 12px; }\n"
  ".lozenge { min-height: 40px; min-width: 40px; border-radius: 999px; padding: 0 12px; font-weight: 800; }\n"
  ".lz-neutral { background-color: alpha(currentColor, 0.1); }\n"
  ".lz-ok { color: var(--success-color); background-color: alpha(var(--success-color), 0.16); }\n"
  ".lz-warn { color: var(--warning-color); background-color: alpha(var(--warning-color), 0.18); }\n"
  ".lz-blue { color: var(--accent-color); background-color: alpha(var(--accent-color), 0.16); }\n"
  ".score-card { padding: 20px 24px; }\n"
  ".score-big { font-size: 34pt; font-weight: 800; }\n"
  ".rating-bar trough, .rating-bar progress { min-height: 6px; }\n"
  ".rating-bar progress { background-color: #e5a50a; }\n"
  ".review-row { padding: 12px 14px; }\n";

static void
load_css (void)
{
  GtkCssProvider *provider = gtk_css_provider_new ();

  gtk_css_provider_load_from_string (provider, base_css);
  gtk_style_context_add_provider_for_display (gdk_display_get_default (), GTK_STYLE_PROVIDER (provider),
                                              GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
  g_object_unref (provider);
}

static void
on_dark (GSimpleAction *action, GVariant *state, gpointer data)
{
  adw_style_manager_set_color_scheme (adw_style_manager_get_default (),
                                      g_variant_get_boolean (state) ? ADW_COLOR_SCHEME_FORCE_DARK
                                                                    : ADW_COLOR_SCHEME_FORCE_LIGHT);
  g_simple_action_set_state (action, state);
}

static void
on_about (GSimpleAction *action, GVariant *param, gpointer app)
{
  AdwDialog *about = adw_about_dialog_new ();
  adw_about_dialog_set_application_name (ADW_ABOUT_DIALOG (about), "Sawmill");
  adw_about_dialog_set_application_icon (ADW_ABOUT_DIALOG (about), APP_ID);
  adw_about_dialog_set_version (ADW_ABOUT_DIALOG (about), *SAWMILL_RELEASE_TAG ? SAWMILL_RELEASE_TAG : "development build");
  adw_about_dialog_set_developer_name (ADW_ABOUT_DIALOG (about), "Sawmill mockup");
  adw_about_dialog_set_license_type (ADW_ABOUT_DIALOG (about), GTK_LICENSE_MIT_X11);
  adw_about_dialog_set_website (ADW_ABOUT_DIALOG (about), "https://github.com/kuhwar/sawmill");
  adw_about_dialog_set_issue_url (ADW_ABOUT_DIALOG (about), "https://github.com/kuhwar/sawmill/issues");
  adw_about_dialog_set_comments (ADW_ABOUT_DIALOG (about),
                                 "Find, install and update AppImages published on GitHub, GitLab and Codeberg.");
  adw_dialog_present (about, GTK_WIDGET (gtk_application_get_active_window (GTK_APPLICATION (app))));
}

static void
on_search (GSimpleAction *action, GVariant *param, gpointer app)
{
  store_window_toggle_search ();
}

static void
on_refresh (GSimpleAction *action, GVariant *param, gpointer app)
{
  store_window_refresh_catalog ();
}

static void
on_check_updates (GSimpleAction *action, GVariant *param, gpointer app)
{
  store_window_check_updates ();
}

static void
on_quit (GSimpleAction *action, GVariant *param, gpointer app)
{
  g_application_quit (G_APPLICATION (app));
}

static void
on_startup (GApplication *app, gpointer data)
{
  gboolean dark = adw_style_manager_get_dark (adw_style_manager_get_default ());
  const GActionEntry entries[] = {
    { "about",  on_about,  NULL, NULL, NULL, { 0 } },
    { "search", on_search, NULL, NULL, NULL, { 0 } },
    { "refresh", on_refresh, NULL, NULL, NULL, { 0 } },
    { "check-updates", on_check_updates, NULL, NULL, NULL, { 0 } },
    { "quit",   on_quit,   NULL, NULL, NULL, { 0 } },
  };
  GSimpleAction *dark_action = g_simple_action_new_stateful ("dark", NULL, g_variant_new_boolean (dark));
  const char *search_accels[] = { "<Ctrl>f", NULL };
  const char *quit_accels[] = { "<Ctrl>q", NULL };
  const char *refresh_accels[] = { "<Ctrl>r", NULL };

  load_css ();
  gtk_window_set_default_icon_name (APP_ID);

  g_action_map_add_action_entries (G_ACTION_MAP (app), entries, G_N_ELEMENTS (entries), app);
  g_signal_connect (dark_action, "change-state", G_CALLBACK (on_dark), NULL);
  g_action_map_add_action (G_ACTION_MAP (app), G_ACTION (dark_action));
  g_object_unref (dark_action);

  gtk_application_set_accels_for_action (GTK_APPLICATION (app), "app.search", search_accels);
  gtk_application_set_accels_for_action (GTK_APPLICATION (app), "app.quit", quit_accels);
  gtk_application_set_accels_for_action (GTK_APPLICATION (app), "app.refresh", refresh_accels);
}

static gboolean
debug_screenshot (gpointer app)
{
  const char *path = g_getenv ("SAWMILL_SCREENSHOT");
  if (!store_window_debug_screenshot (path))
    g_printerr ("Could not save screenshot to %s\n", path);
  g_application_quit (G_APPLICATION (app));
  return G_SOURCE_REMOVE;
}

static void
on_activate (GApplication *app, gpointer data)
{
  GtkWindow *win = gtk_application_get_active_window (GTK_APPLICATION (app));
  const char *open = g_getenv ("SAWMILL_OPEN");

  if (!win)
    win = GTK_WINDOW (store_window_new (ADW_APPLICATION (app)));
  gtk_window_present (win);

  if (open && *open)
    store_window_debug_open (open);
  if (g_getenv ("SAWMILL_SCREENSHOT"))
    {
      const char *delay = g_getenv ("SAWMILL_SCREENSHOT_DELAY_MS");
      g_timeout_add (delay ? (guint) atoi (delay) : 1500, debug_screenshot, app);
    }
}

int
main (int argc, char **argv)
{
  g_autoptr (AdwApplication) app = adw_application_new (APP_ID, G_APPLICATION_DEFAULT_FLAGS);

  g_set_application_name ("Sawmill");
  g_signal_connect (app, "startup", G_CALLBACK (on_startup), NULL);
  g_signal_connect (app, "activate", G_CALLBACK (on_activate), NULL);
  return g_application_run (G_APPLICATION (app), argc, argv);
}
