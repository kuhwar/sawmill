#pragma once

#include <gtk/gtk.h>
#include "store-catalog.h"

G_BEGIN_DECLS

typedef enum {
  GLYPH_NOTE, GLYPH_LEAF, GLYPH_CAMERA, GLYPH_CODE, GLYPH_CHAT, GLYPH_GLOBE,
  GLYPH_BRUSH, GLYPH_CHART, GLYPH_PIN, GLYPH_BOOK, GLYPH_GAME, GLYPH_TERM,
  GLYPH_CLOUD, GLYPH_MIC, GLYPH_TILES, GLYPH_LOCK
} StoreGlyph;

/* The app's icon.png at the given size (falls back to a generic icon). */
GtkWidget  *store_app_icon_new    (StoreApp   *app,
                                   int         size);

/* A cairo-drawn symbolic glyph in the widget's CSS colour. */
GtkWidget  *store_glyph_icon_new  (StoreGlyph  glyph,
                                   int         size);
StoreGlyph  store_category_glyph  (const char *category_id);

G_END_DECLS
