/* Reads the plain-text catalog (catalog/<host>/<owner>/<repo>/<property>) into memory. */

#include <string.h>
#include "store-catalog.h"

G_DEFINE_QUARK (store-catalog-error-quark, store_catalog_error)

/* ---------- file helpers ---------- */

static char *
read_file (const char *dir, const char *name)
{
  g_autofree char *path = g_build_filename (dir, name, NULL);
  char *text = NULL;

  if (!g_file_get_contents (path, &text, NULL, NULL))
    return NULL;
  if (!g_utf8_validate (text, -1, NULL))
    {
      g_free (text);
      return NULL;
    }
  return text;
}

static char *
read_line (const char *dir, const char *name)
{
  char *text = read_file (dir, name);
  if (text)
    g_strstrip (text);
  if (text && !*text)
    g_clear_pointer (&text, g_free);
  return text;
}

static GStrv
read_list (const char *dir, const char *name)
{
  g_autofree char *text = read_file (dir, name);
  g_autoptr (GStrvBuilder) b = g_strv_builder_new ();

  if (text)
    {
      g_auto (GStrv) parts = g_strsplit_set (text, ",\n", -1);
      for (guint i = 0; parts[i]; i++)
        {
          g_strstrip (parts[i]);
          if (*parts[i])
            g_strv_builder_add (b, parts[i]);
        }
    }
  return g_strv_builder_end (b);
}

static GStrv
tokens (const char *text)
{
  g_auto (GStrv) ascii = NULL;
  g_auto (GStrv) toks = NULL;
  g_autoptr (GStrvBuilder) b = g_strv_builder_new ();

  if (!text)
    return g_strv_builder_end (g_steal_pointer (&b));
  toks = g_str_tokenize_and_fold (text, NULL, &ascii);
  g_strv_builder_addv (b, (const char **) toks);
  g_strv_builder_addv (b, (const char **) ascii);
  return g_strv_builder_end (b);
}

static GStrv
tokens_of_list (GStrv items)
{
  g_autofree char *joined = g_strjoinv (" ", items);
  return tokens (joined);
}

/* ---------- loading ---------- */

static void
store_app_free (StoreApp *a)
{
  g_free (a->id); g_free (a->host); g_free (a->owner); g_free (a->repo); g_free (a->dir);
  g_free (a->title); g_free (a->summary); g_free (a->description);
  g_strfreev (a->keywords); g_strfreev (a->categories); g_strfreev (a->thumbnails);
  g_free (a->developer); g_free (a->license); g_free (a->age_rating); g_free (a->homepage);
  g_free (a->asset_x86_64); g_free (a->asset_aarch64); g_free (a->tag_pattern); g_free (a->installed_version);
  g_clear_object (&a->job);
  g_strfreev (a->tok_title); g_strfreev (a->tok_keywords); g_strfreev (a->tok_categories);
  g_strfreev (a->tok_developer); g_strfreev (a->tok_summary); g_strfreev (a->tok_description);
  g_clear_object (&a->icon);
  g_clear_pointer (&a->watchers, g_ptr_array_unref);
  g_free (a);
}

static void
store_category_free (StoreCategory *c)
{
  g_free (c->id); g_free (c->name); g_free (c->description);
  g_free (c);
}

static StoreApp *
load_app (const char *dir, const char *host, const char *owner, const char *repo, GError **error)
{
  StoreApp *a = g_new0 (StoreApp, 1);
  g_autofree char *desc = NULL;
  g_autofree char *icon = g_build_filename (dir, "icon.png", NULL);
  const char *missing = NULL;
  char *nl;

  a->dir = g_strdup (dir);
  a->host = g_strdup (host);
  a->owner = g_strdup (owner);
  a->repo = g_strdup (repo);
  a->id = g_strdup_printf ("%s/%s/%s", host, owner, repo);
  a->title = read_line (dir, "title");
  desc = read_file (dir, "description");
  a->keywords = read_list (dir, "keywords");
  a->categories = read_list (dir, "categories");
  a->thumbnails = read_list (dir, "thumbnails");
  a->developer = read_line (dir, "developer");
  a->license = read_line (dir, "license");
  a->age_rating = read_line (dir, "age-rating");
  a->homepage = read_line (dir, "homepage");
  a->tag_pattern = read_line (dir, "tag-pattern");
  {
    g_autofree char *pre = read_line (dir, "prereleases");
    g_autofree char *assets = read_file (dir, "assets");
    a->prereleases = g_strcmp0 (pre, "yes") == 0;
    if (assets)
      {
        g_auto (GStrv) lines = g_strsplit (assets, "\n", -1);
        for (guint i = 0; lines[i]; i++)
          {
            char *line = g_strstrip (lines[i]), *sp = strchr (line, ' ');
            if (!sp)
              continue;
            *sp = '\0';
            if (g_str_equal (line, "x86_64"))
              a->asset_x86_64 = g_strstrip (g_strdup (sp + 1));
            else if (g_str_equal (line, "aarch64"))
              a->asset_aarch64 = g_strstrip (g_strdup (sp + 1));
          }
      }
  }

  if (!a->title) missing = "title";
  else if (!desc) missing = "description";
  else if (!a->categories[0]) missing = "categories";
  else if (!a->developer) missing = "developer";
  else if (!a->license) missing = "license";
  else if (!a->age_rating) missing = "age-rating";
  else if (!g_file_test (icon, G_FILE_TEST_IS_REGULAR)) missing = "icon.png";
  if (missing)
    {
      g_set_error (error, STORE_CATALOG_ERROR, STORE_CATALOG_ERROR_INVALID, "%s: missing %s", a->id, missing);
      store_app_free (a);
      return NULL;
    }
  if (a->homepage && !g_str_has_prefix (a->homepage, "https://"))
    g_clear_pointer (&a->homepage, g_free);

  /* description: summary line, blank line, full text */
  nl = strchr (desc, '\n');
  if (nl)
    *nl = '\0';
  a->summary = g_strstrip (g_strdup (desc));
  a->description = g_strstrip (g_strdup (nl ? nl + 1 : ""));

  a->tok_title = tokens (a->title);
  a->tok_keywords = tokens_of_list (a->keywords);
  a->tok_developer = tokens (a->developer);
  a->tok_summary = tokens (a->summary);
  a->tok_description = tokens (a->description);
  a->watchers = g_ptr_array_new_with_free_func (g_free);
  a->state = APP_AVAILABLE;
  return a;
}

static GPtrArray *
load_categories (const char *dir)
{
  g_autofree char *text = read_file (dir, "categories");
  GPtrArray *cats = g_ptr_array_new_with_free_func ((GDestroyNotify) store_category_free);
  g_auto (GStrv) lines = NULL;

  if (!text)
    return cats;
  lines = g_strsplit (text, "\n", -1);
  for (guint i = 0; lines[i]; i++)
    {
      g_auto (GStrv) f = NULL;
      StoreCategory *c;

      if (!*g_strstrip (lines[i]) || lines[i][0] == '#')
        continue;
      f = g_strsplit (lines[i], "|", 3);
      if (!f[0] || !f[1])
        continue;
      c = g_new0 (StoreCategory, 1);
      c->id = g_strstrip (g_strdup (f[0]));
      c->name = g_strstrip (g_strdup (f[1]));
      c->description = g_strstrip (g_strdup (f[2] ? f[2] : ""));
      g_ptr_array_add (cats, c);
    }
  return cats;
}

static int
compare_title (gconstpointer pa, gconstpointer pb)
{
  const StoreApp *a = *(StoreApp **) pa, *b = *(StoreApp **) pb;
  g_autofree char *ka = g_utf8_collate_key (a->title, -1);
  g_autofree char *kb = g_utf8_collate_key (b->title, -1);
  return strcmp (ka, kb);
}

StoreCatalog *
store_catalog_load (const char *dir, GError **error)
{
  g_autofree char *format = read_line (dir, "format");
  g_autofree char *featured = NULL;
  g_autoptr (GDir) hosts = NULL;
  StoreCatalog *cat;
  const char *host;

  if (!format)
    {
      g_set_error (error, STORE_CATALOG_ERROR, STORE_CATALOG_ERROR_MISSING, "No catalog found in %s", dir);
      return NULL;
    }
  if (!g_str_equal (format, STORE_CATALOG_FORMAT))
    {
      g_set_error (error, STORE_CATALOG_ERROR, STORE_CATALOG_ERROR_TOO_NEW,
                   "The catalog uses format %s, but this version of Sawmill understands format %s",
                   format, STORE_CATALOG_FORMAT);
      return NULL;
    }
  hosts = g_dir_open (dir, 0, error);
  if (!hosts)
    return NULL;

  cat = g_new0 (StoreCatalog, 1);
  cat->dir = g_strdup (dir);
  cat->apps = g_ptr_array_new_with_free_func ((GDestroyNotify) store_app_free);
  cat->categories = load_categories (dir);
  cat->banner = g_ptr_array_new ();
  cat->picks = g_ptr_array_new ();

  /* <host>/<owner>/<repo> */
  while ((host = g_dir_read_name (hosts)))
    {
      g_autofree char *host_dir = g_build_filename (dir, host, NULL);
      g_autoptr (GDir) owners = NULL;
      const char *owner;

      if (!strchr (host, '.') || !(owners = g_dir_open (host_dir, 0, NULL)))
        continue;
      while ((owner = g_dir_read_name (owners)))
        {
          g_autofree char *owner_dir = g_build_filename (host_dir, owner, NULL);
          g_autoptr (GDir) repos = g_dir_open (owner_dir, 0, NULL);
          const char *repo;

          while (repos && (repo = g_dir_read_name (repos)))
            {
              g_autofree char *app_dir = g_build_filename (owner_dir, repo, NULL);
              g_autoptr (GError) err = NULL;
              StoreApp *a;

              if (!g_file_test (app_dir, G_FILE_TEST_IS_DIR))
                continue;
              a = load_app (app_dir, host, owner, repo, &err);
              if (a)
                g_ptr_array_add (cat->apps, a);
              else
                {
                  g_warning ("Skipping catalog entry %s", err->message);
                  cat->skipped++;
                }
            }
        }
    }
  g_ptr_array_sort (cat->apps, compare_title);

  /* category names are searchable too */
  for (guint i = 0; i < cat->apps->len; i++)
    {
      StoreApp *a = g_ptr_array_index (cat->apps, i);
      g_autoptr (GStrvBuilder) b = g_strv_builder_new ();
      g_auto (GStrv) names = NULL;
      for (guint k = 0; a->categories[k]; k++)
        {
          g_strv_builder_add (b, a->categories[k]);
          g_strv_builder_add (b, store_catalog_category_name (cat, a->categories[k]));
        }
      names = g_strv_builder_end (b);
      a->tok_categories = tokens_of_list (names);
    }

  featured = read_file (dir, "featured");
  if (featured)
    {
      g_auto (GStrv) lines = g_strsplit (featured, "\n", -1);
      for (guint i = 0; lines[i]; i++)
        {
          g_auto (GStrv) f = g_strsplit (g_strstrip (lines[i]), " ", 2);
          StoreApp *a;
          if (!f[0] || !f[1] || f[0][0] == '#' || !(a = store_catalog_find (cat, g_strstrip (f[1]))))
            continue;
          if (g_str_equal (f[0], "banner"))
            g_ptr_array_add (cat->banner, a);
          else if (g_str_equal (f[0], "pick"))
            g_ptr_array_add (cat->picks, a);
        }
    }
  return cat;
}

StoreApp *
store_catalog_find (StoreCatalog *cat, const char *id)
{
  for (guint i = 0; i < cat->apps->len; i++)
    {
      StoreApp *a = g_ptr_array_index (cat->apps, i);
      if (g_ascii_strcasecmp (a->id, id) == 0)
        return a;
    }
  return NULL;
}

const StoreCategory *
store_catalog_category (StoreCatalog *cat, const char *id)
{
  for (guint i = 0; i < cat->categories->len; i++)
    {
      StoreCategory *c = g_ptr_array_index (cat->categories, i);
      if (g_str_equal (c->id, id))
        return c;
    }
  return NULL;
}

const char *
store_catalog_category_name (StoreCatalog *cat, const char *id)
{
  const StoreCategory *c = store_catalog_category (cat, id);
  return c ? c->name : id;
}

/* ---------- search ---------- */

typedef struct {
  StoreApp *app;
  int       score;
} Hit;

static gboolean
prefix_in (GStrv toks, const char *word, gboolean *exact)
{
  gboolean found = FALSE;
  for (guint i = 0; toks && toks[i]; i++)
    if (g_str_has_prefix (toks[i], word))
      {
        found = TRUE;
        if (g_str_equal (toks[i], word))
          {
            *exact = TRUE;
            return TRUE;
          }
      }
  return found;
}

static int
word_score (StoreApp *a, const char *word)
{
  static const struct { gsize offset; int score; } fields[] = {
    { G_STRUCT_OFFSET (StoreApp, tok_title),       100 },
    { G_STRUCT_OFFSET (StoreApp, tok_keywords),     60 },
    { G_STRUCT_OFFSET (StoreApp, tok_categories),   40 },
    { G_STRUCT_OFFSET (StoreApp, tok_developer),    40 },
    { G_STRUCT_OFFSET (StoreApp, tok_summary),      20 },
    { G_STRUCT_OFFSET (StoreApp, tok_description),   5 },
  };
  for (guint i = 0; i < G_N_ELEMENTS (fields); i++)
    {
      GStrv toks = G_STRUCT_MEMBER (GStrv, a, fields[i].offset);
      gboolean exact = FALSE;
      if (prefix_in (toks, word, &exact))
        return exact ? fields[i].score + fields[i].score / 4 : fields[i].score;
    }
  return 0;
}

static int
compare_hits (gconstpointer pa, gconstpointer pb)
{
  const Hit *a = pa, *b = pb;
  if (a->score != b->score)
    return b->score - a->score;
  return g_utf8_collate (a->app->title, b->app->title);
}

GPtrArray *
store_catalog_search (StoreCatalog *cat, const char *query)
{
  g_auto (GStrv) words = g_str_tokenize_and_fold (query, NULL, NULL);
  g_autoptr (GArray) hits = g_array_new (FALSE, FALSE, sizeof (Hit));
  g_autofree char *folded_query = g_utf8_casefold (query, -1);
  GPtrArray *out = g_ptr_array_new ();

  if (!words[0])
    return out;

  for (guint i = 0; i < cat->apps->len; i++)
    {
      StoreApp *a = g_ptr_array_index (cat->apps, i);
      g_autofree char *folded_title = g_utf8_casefold (a->title, -1);
      Hit h = { a, 0 };

      for (guint w = 0; words[w]; w++)
        {
          int s = word_score (a, words[w]);
          if (s == 0)
            {
              h.score = 0;
              break;
            }
          h.score += s;
        }
      if (h.score > 0 && g_str_equal (g_strstrip (folded_title), g_strstrip (folded_query)))
        h.score += 200;     /* typed the exact name */
      if (h.score > 0)
        g_array_append_val (hits, h);
    }
  g_array_sort (hits, compare_hits);
  for (guint i = 0; i < hits->len; i++)
    g_ptr_array_add (out, g_array_index (hits, Hit, i).app);
  return out;
}

/* ---------- icons ---------- */

GdkTexture *
store_app_get_icon (StoreApp *a)
{
  if (!a->icon_tried)
    {
      g_autofree char *path = g_build_filename (a->dir, "icon.png", NULL);
      g_autoptr (GError) err = NULL;
      a->icon_tried = TRUE;
      a->icon = gdk_texture_new_from_filename (path, &err);
      if (!a->icon)
        g_warning ("%s: cannot load icon: %s", a->id, err->message);
    }
  return a->icon;
}

/* Average colour of the icon's opaque pixels, darkened until white text is readable on it. */
const GdkRGBA *
store_app_get_accent (StoreApp *a)
{
  GdkTexture *tex;

  if (a->accent_ready)
    return &a->accent;
  a->accent_ready = TRUE;
  a->accent = (GdkRGBA) { 0.21, 0.52, 0.89, 1 };
  tex = store_app_get_icon (a);
  if (tex)
    {
      int w = gdk_texture_get_width (tex), h = gdk_texture_get_height (tex);
      g_autofree guchar *px = g_malloc ((gsize) w * h * 4);
      double r = 0, g = 0, b = 0, n = 0;

      gdk_texture_download (tex, px, (gsize) w * 4);     /* B8G8R8A8 premultiplied (little-endian ARGB32) */
      for (int i = 0; i < w * h; i++)
        {
          guchar *p = px + i * 4;
          double alpha = p[3] / 255.0;
          double lo = MIN (p[0], MIN (p[1], p[2])) / 255.0, hi = MAX (p[0], MAX (p[1], p[2])) / 255.0;
          double sat = alpha > 0 ? (hi - lo) / alpha : 0;
          double weight = alpha > 0.9 ? 0.2 + sat * sat * 4 : 0;   /* prefer colourful pixels */
          if (weight <= 0)
            continue;
          b += p[0] / 255.0 / alpha * weight;
          g += p[1] / 255.0 / alpha * weight;
          r += p[2] / 255.0 / alpha * weight;
          n += weight;
        }
      if (n > 0)
        {
          double lum;
          a->accent = (GdkRGBA) { r / n, g / n, b / n, 1 };
          lum = 0.2126 * a->accent.red + 0.7152 * a->accent.green + 0.0722 * a->accent.blue;
          if (lum > 0.42)
            {
              double k = 0.42 / lum;
              a->accent.red *= k; a->accent.green *= k; a->accent.blue *= k;
            }
        }
    }
  return &a->accent;
}

char *
store_app_source_url (StoreApp *a)
{
  return g_strdup_printf ("https://%s", a->id);
}

gboolean
store_app_is_free_software (StoreApp *a)
{
  return !g_str_has_prefix (a->license, "LicenseRef-");
}
