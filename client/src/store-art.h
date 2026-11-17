#pragma once

#include <gtk/gtk.h>
#include "store-data.h"

G_BEGIN_DECLS

/* Generated placeholder artwork, drawn with cairo. */
GtkWidget *store_app_icon_new      (StoreApp   *app,
                                    int         size);
GtkWidget *store_glyph_icon_new    (StoreGlyph  glyph,
                                    int         size);
GtkWidget *store_screenshot_new    (StoreApp   *app,
                                    StoreShot   layout,
                                    int         height,
                                    double      xalign,
                                    double      yalign);

G_END_DECLS
