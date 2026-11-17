#pragma once

#include <adwaita.h>

G_BEGIN_DECLS

GtkWidget *store_window_new           (AdwApplication *app);
void       store_window_toggle_search (void);

/* developer helpers, driven by SAWMILL_OPEN and SAWMILL_SCREENSHOT */
void       store_window_debug_open       (const char *what);
gboolean   store_window_debug_screenshot (const char *path);

G_END_DECLS
