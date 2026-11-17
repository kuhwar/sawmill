#pragma once

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define STORE_CATALOG_FORMAT "1"

#define STORE_CATALOG_ERROR (store_catalog_error_quark ())
GQuark store_catalog_error_quark (void);

typedef enum {
  STORE_CATALOG_ERROR_MISSING,    /* no catalog at that path */
  STORE_CATALOG_ERROR_TOO_NEW,    /* written for a newer client */
  STORE_CATALOG_ERROR_INVALID,
} StoreCatalogError;

typedef enum {
  APP_AVAILABLE,
  APP_INSTALLED,
  APP_UPDATE,
} StoreAppState;

typedef struct {
  char *id;
  char *name;
  char *description;
} StoreCategory;

typedef struct {
  /* from the catalog: catalog/<host>/<owner>/<repo>/ */
  char  *id;               /* "github.com/owner/repo" */
  char  *host, *owner, *repo;
  char  *dir;
  char  *title;
  char  *summary;          /* first line of description */
  char  *description;      /* the rest, may be "" */
  GStrv  keywords;
  GStrv  categories;
  GStrv  thumbnails;       /* https URLs, may be empty */
  char  *developer;
  char  *license;
  char  *age_rating;
  char  *homepage;         /* NULL when absent */

  /* search: folded tokens per field */
  GStrv  tok_title, tok_keywords, tok_categories, tok_developer, tok_summary, tok_description;

  /* lazily loaded */
  GdkTexture *icon;
  gboolean    icon_tried;
  GdkRGBA     accent;
  gboolean    accent_ready;

  /* runtime state (simulated until installing is implemented) */
  StoreAppState state;
  GPtrArray    *watchers;
  gboolean      busy;
  gboolean      busy_is_update;
  double        progress;
} StoreApp;

typedef struct {
  char      *dir;
  GPtrArray *apps;          /* StoreApp*, sorted by title */
  GPtrArray *categories;    /* StoreCategory*, in file order */
  GPtrArray *banner;        /* StoreApp*, borrowed */
  GPtrArray *picks;         /* StoreApp*, borrowed */
  guint      skipped;       /* entries that could not be read */
} StoreCatalog;

StoreCatalog        *store_catalog_load            (const char    *dir,
                                                    GError       **error);
StoreApp            *store_catalog_find            (StoreCatalog  *catalog,
                                                    const char    *id);
const StoreCategory *store_catalog_category        (StoreCatalog  *catalog,
                                                    const char    *id);
const char          *store_catalog_category_name   (StoreCatalog  *catalog,
                                                    const char    *id);

/* Apps matching every word of the query, best first. Returns a new array of borrowed pointers. */
GPtrArray           *store_catalog_search          (StoreCatalog  *catalog,
                                                    const char    *query);

GdkTexture          *store_app_get_icon            (StoreApp      *app);
const GdkRGBA       *store_app_get_accent          (StoreApp      *app);
char                *store_app_source_url          (StoreApp      *app);
gboolean             store_app_is_free_software    (StoreApp      *app);

G_END_DECLS
