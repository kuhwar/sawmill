#pragma once

#include <gtk/gtk.h>

G_BEGIN_DECLS

typedef enum {
  GLYPH_NOTE, GLYPH_LEAF, GLYPH_CAMERA, GLYPH_CODE, GLYPH_CHAT, GLYPH_GLOBE,
  GLYPH_BRUSH, GLYPH_CHART, GLYPH_PIN, GLYPH_BOOK, GLYPH_GAME, GLYPH_TERM,
  GLYPH_CLOUD, GLYPH_MIC, GLYPH_TILES, GLYPH_LOCK
} StoreGlyph;

typedef enum {
  SHOT_GRID, SHOT_LIST, SHOT_DOC, SHOT_CHART, SHOT_PLAYER, SHOT_MAP, SHOT_GAME
} StoreShot;

typedef enum {
  APP_AVAILABLE, APP_INSTALLED, APP_UPDATE
} StoreAppState;

typedef struct {
  const char *id;
  const char *name;
  StoreGlyph  glyph;
} StoreCategory;

typedef struct {
  const char   *id;
  const char   *name;
  const char   *developer;
  gboolean      verified;
  const char   *category;
  StoreGlyph    glyph;
  const char   *color1;
  const char   *color2;
  const char   *summary;
  double        size_mb;
  double        rating;
  int           votes;
  StoreAppState state;
  const char   *version;
  gboolean      free_software;
  gboolean      safe;
  gboolean      adaptive;
  StoreShot     shots[3];
  const char   *description;
  const char   *features[4];

  /* runtime */
  GdkRGBA     c1, c2;
  GPtrArray  *watchers;
  gboolean    busy;
  gboolean    busy_is_update;
  double      progress;
} StoreApp;

#define STORE_N_SHOTS 3

extern StoreApp      store_apps[];
extern const guint   store_n_apps;
extern StoreCategory store_categories[];
extern const guint   store_n_categories;
extern const char   *store_featured[];
extern const guint   store_n_featured;
extern const char   *store_picks[];
extern const guint   store_n_picks;
extern const char   *store_new[];
extern const guint   store_n_new;

typedef struct {
  const char *title;
  const char *body;
  int         stars;
} StoreReview;

extern const StoreReview store_reviews[];
extern const guint       store_n_reviews;
extern const char       *store_reviewers[];
extern const guint       store_n_reviewers;

void                 store_data_init        (void);
StoreApp            *store_app_by_id        (const char *id);
const StoreCategory *store_category_by_id   (const char *id);

G_END_DECLS
