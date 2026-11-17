/* Fictional catalogue for the Sawmill store mockup. */

#include "store-data.h"

StoreApp store_apps[] = {
  { "tidewave", "Tidewave", "Marlin Collective", TRUE, "audio", GLYPH_NOTE, "#ff7a59", "#c0305a",
    "Play your local music library with gapless playback", 18.4, 4.7, 1284, APP_INSTALLED, "3.2.1", TRUE, TRUE, TRUE,
    { SHOT_PLAYER, SHOT_GRID, SHOT_LIST },
    "Tidewave is a music player built for big local libraries. It indexes thousands of albums in seconds and keeps your listening going with gapless playback and smart queues.",
    { "Gapless and crossfade playback", "Synced lyrics from embedded tags", "Replay gain and a 10-band equalizer", "MPRIS support for media keys" } },
  { "fernlog", "Fernlog", "Opaline Labs", TRUE, "work", GLYPH_LEAF, "#5fd38d", "#1b7f5a",
    "Plain-text notes that link together like a garden", 9.1, 4.8, 2210, APP_AVAILABLE, "1.9.0", TRUE, TRUE, TRUE,
    { SHOT_DOC, SHOT_LIST, SHOT_GRID },
    "Fernlog keeps your notes as Markdown files in a folder you choose. Link notes with double brackets and watch a web of ideas grow in the graph view.",
    { "Backlinks and a live graph view", "Works offline with plain files", "Daily journal with templates", "Sync through any folder service" } },
  { "lumen", "Lumen Darkroom", "Halide Works", TRUE, "create", GLYPH_CAMERA, "#ffbe3d", "#d96a0b",
    "Develop RAW photos with non-destructive edits", 64.7, 4.5, 873, APP_UPDATE, "5.0.2", TRUE, TRUE, FALSE,
    { SHOT_GRID, SHOT_CHART, SHOT_PLAYER },
    "Lumen Darkroom reads RAW files from more than 600 cameras. Every adjustment is stored as a recipe, so you can revisit a photo years later and change your mind.",
    { "Curves, masks and color grading", "Batch export with presets", "Tethered shooting for supported cameras", "Lens correction profiles" } },
  { "quarry", "Quarry", "Basalt Software", FALSE, "develop", GLYPH_CODE, "#7c8cff", "#3c3fb5",
    "A fast code editor with built-in language servers", 42.3, 4.6, 1560, APP_INSTALLED, "0.48.0", TRUE, FALSE, FALSE,
    { SHOT_DOC, SHOT_LIST, SHOT_CHART },
    "Quarry opens large projects instantly and talks to language servers out of the box. Split panes, multiple cursors and a built-in terminal keep you in flow.",
    { "Language servers for 40+ languages", "Multiple cursors and split panes", "Integrated Git gutter", "Extensions written in Lua" } },
  { "burrow", "Burrow", "Warren Chat", TRUE, "social", GLYPH_CHAT, "#4fc3f7", "#1565c0",
    "Private, end-to-end encrypted group chat", 31.0, 4.3, 642, APP_AVAILABLE, "2.4.0", TRUE, FALSE, TRUE,
    { SHOT_LIST, SHOT_DOC, SHOT_GRID },
    "Burrow is a messenger for friends, families and small teams. Every message, call and file is end-to-end encrypted, and your account works across phone and desktop.",
    { "Encrypted voice and video calls", "Threads and reactions", "Disappearing messages", "Adaptive layout for phones" } },
  { "driftwood", "Driftwood", "Coastline Browser Co.", FALSE, "utils", GLYPH_GLOBE, "#26c6da", "#00796b",
    "A calm web browser with tracker blocking built in", 88.9, 4.4, 3021, APP_AVAILABLE, "128.2", TRUE, FALSE, TRUE,
    { SHOT_GRID, SHOT_DOC, SHOT_LIST },
    "Driftwood blocks trackers and cookie banners by default and keeps your tabs tidy with vertical tab groups.",
    { "Tracker and banner blocking", "Vertical tab groups", "Reader mode", "Sync bookmarks with a passphrase" } },
  { "inkwell", "Inkwell", "Quill & Pixel", TRUE, "create", GLYPH_BRUSH, "#f06292", "#8e24aa",
    "Sketch and paint with pressure-sensitive brushes", 27.5, 4.9, 991, APP_AVAILABLE, "2.1.0", TRUE, TRUE, TRUE,
    { SHOT_PLAYER, SHOT_GRID, SHOT_CHART },
    "Inkwell turns your tablet into a sketchbook. Brushes respond to pressure and tilt, and an infinite canvas means you never run out of paper.",
    { "Pressure and tilt-aware brushes", "Infinite canvas with layers", "Export to PNG, SVG and ORA", "Timelapse recording" } },
  { "ledgerline", "Ledgerline", "Tally House", FALSE, "work", GLYPH_CHART, "#9ccc65", "#33691e",
    "Track spending and budgets without a cloud account", 14.2, 4.2, 318, APP_INSTALLED, "1.3.4", FALSE, TRUE, TRUE,
    { SHOT_CHART, SHOT_LIST, SHOT_GRID },
    "Ledgerline imports bank statements and sorts them into budgets you define. Everything stays on your computer.",
    { "Imports OFX, QIF and CSV", "Envelope-style budgets", "Monthly reports and trends", "Encrypted local database" } },
  { "waypoint", "Waypoint", "Contour Maps", TRUE, "utils", GLYPH_PIN, "#ff8a65", "#d84315",
    "Offline maps and trail routes for hiking", 22.8, 4.6, 1102, APP_UPDATE, "4.7.0", TRUE, FALSE, TRUE,
    { SHOT_MAP, SHOT_LIST, SHOT_CHART },
    "Waypoint downloads whole regions for offline use and plans routes along footpaths and cycle tracks, with elevation profiles for every route.",
    { "Offline vector maps", "Elevation profiles", "GPX import and export", "Contour lines and hillshade" } },
  { "paperback", "Paperback", "Folio Readers", FALSE, "learn", GLYPH_BOOK, "#bcaaa4", "#6d4c41",
    "Read EPUB and PDF books with highlights and notes", 12.6, 4.5, 770, APP_AVAILABLE, "3.0.1", TRUE, TRUE, TRUE,
    { SHOT_DOC, SHOT_GRID, SHOT_LIST },
    "Paperback is a quiet place to read. It remembers where you stopped in every book and collects your highlights in one place.",
    { "EPUB, PDF and CBZ support", "Highlights with export to Markdown", "Night and sepia modes", "Dictionary lookup" } },
  { "orchard", "Pixel Orchard", "Seedling Games", FALSE, "play", GLYPH_GAME, "#ffd54f", "#f57f17",
    "Grow a cozy farm one season at a time", 156.0, 4.8, 2488, APP_AVAILABLE, "1.2.0", FALSE, TRUE, FALSE,
    { SHOT_GAME, SHOT_GRID, SHOT_PLAYER },
    "Plant, harvest and trade in a little valley that changes with the seasons. Play solo or invite a friend for local co-op.",
    { "Four seasons with unique crops", "Local two-player co-op", "Controller support", "Hand-drawn pixel art" } },
  { "shellfish", "Shellfish", "Tidepool Dev", TRUE, "develop", GLYPH_TERM, "#90a4ae", "#37474f",
    "A GPU-accelerated terminal with tabs and splits", 7.9, 4.7, 1340, APP_INSTALLED, "1.6.2", TRUE, FALSE, FALSE,
    { SHOT_DOC, SHOT_LIST, SHOT_GRID },
    "Shellfish renders on the GPU so scrolling through huge logs stays smooth. Tabs, splits and profiles make it easy to keep many sessions organized.",
    { "GPU rendering", "Tabs, splits and profiles", "Ligatures and true color", "Search through scrollback" } },
  { "nimbus", "Nimbus", "Barometer", FALSE, "utils", GLYPH_CLOUD, "#64b5f6", "#1e5bb8",
    "Hourly forecasts and rain radar for your places", 5.4, 4.4, 506, APP_UPDATE, "2.9.0", TRUE, TRUE, TRUE,
    { SHOT_CHART, SHOT_MAP, SHOT_LIST },
    "Nimbus shows the next 48 hours at a glance and animates the rain radar so you know when to grab an umbrella.",
    { "48-hour and 10-day forecasts", "Animated rain radar", "Severe weather alerts", "Saves multiple places" } },
  { "echo", "Echo Chamber", "Lantern Audio", FALSE, "audio", GLYPH_MIC, "#ba68c8", "#5e35b1",
    "Subscribe to podcasts and listen at your pace", 11.3, 4.1, 289, APP_AVAILABLE, "0.9.6", TRUE, FALSE, TRUE,
    { SHOT_LIST, SHOT_PLAYER, SHOT_GRID },
    "Echo Chamber finds podcasts by name or feed URL, downloads new episodes and trims silence so you save time.",
    { "Silence trimming and speed control", "OPML import", "Chapter markers", "Sleep timer" } },
  { "tessellate", "Tessellate", "Hexagon Hobbies", TRUE, "play", GLYPH_TILES, "#4db6ac", "#00695c",
    "A relaxing tile puzzle with 300 handmade levels", 38.2, 4.6, 812, APP_AVAILABLE, "2.0.0", TRUE, TRUE, TRUE,
    { SHOT_GAME, SHOT_GRID, SHOT_CHART },
    "Rotate and slide tiles until the pattern fits. There are no timers and no ads, only 300 handmade levels.",
    { "300 handmade levels", "Daily puzzle", "Color-blind friendly palettes", "Touch and keyboard controls" } },
  { "vault", "Keyring Vault", "Bastion Security", TRUE, "utils", GLYPH_LOCK, "#ef5350", "#a31545",
    "Store passwords and passkeys in an encrypted vault", 16.8, 4.7, 1933, APP_AVAILABLE, "3.4.0", TRUE, TRUE, TRUE,
    { SHOT_LIST, SHOT_DOC, SHOT_GRID },
    "Keyring Vault keeps passwords, passkeys and one-time codes in a single encrypted file that you own.",
    { "Passkeys and TOTP codes", "Password health report", "Browser autofill", "KDBX-compatible format" } },
};
const guint store_n_apps = G_N_ELEMENTS (store_apps);

StoreCategory store_categories[] = {
  { "create",  "Create",        GLYPH_BRUSH },
  { "work",    "Work",          GLYPH_CHART },
  { "play",    "Play",          GLYPH_GAME  },
  { "social",  "Socialize",     GLYPH_CHAT  },
  { "learn",   "Learn",         GLYPH_BOOK  },
  { "develop", "Develop",       GLYPH_CODE  },
  { "audio",   "Audio & Video", GLYPH_NOTE  },
  { "utils",   "Utilities",     GLYPH_TILES },
};
const guint store_n_categories = G_N_ELEMENTS (store_categories);

const char *store_featured[] = { "inkwell", "waypoint", "orchard" };
const guint store_n_featured = G_N_ELEMENTS (store_featured);
const char *store_picks[] = { "fernlog", "burrow", "paperback", "vault", "tessellate", "driftwood" };
const guint store_n_picks = G_N_ELEMENTS (store_picks);
const char *store_new[] = { "echo", "nimbus", "lumen", "quarry", "tidewave", "ledgerline" };
const guint store_n_new = G_N_ELEMENTS (store_new);

const StoreReview store_reviews[] = {
  { "Exactly what I needed", "Fast, tidy and it fits right in with the rest of my desktop. The dark style looks great.", 5 },
  { "Great, with one wish", "Love the design. I would like a few more keyboard shortcuts, but it is already my daily driver.", 4 },
  { "Solid update", "The latest version fixed the startup lag I had on my older laptop. Thanks to the developers.", 5 },
  { "Works well on my phone", "The adaptive layout means I use the same app on my Linux phone and my desktop.", 4 },
  { "Good but young", "A few rough edges here and there, but it improves with every release.", 3 },
};
const guint store_n_reviews = G_N_ELEMENTS (store_reviews);

const char *store_reviewers[] = { "Ana R.", "Tomasz K.", "Priya S.", "Jonas B.", "Mei L.", "Okonkwo E.", "Sofia M.", "Luca D." };
const guint store_n_reviewers = G_N_ELEMENTS (store_reviewers);

void
store_data_init (void)
{
  for (guint i = 0; i < store_n_apps; i++)
    {
      StoreApp *a = &store_apps[i];
      gdk_rgba_parse (&a->c1, a->color1);
      gdk_rgba_parse (&a->c2, a->color2);
      a->watchers = g_ptr_array_new_with_free_func (g_free);
    }
}

StoreApp *
store_app_by_id (const char *id)
{
  for (guint i = 0; i < store_n_apps; i++)
    if (g_str_equal (store_apps[i].id, id))
      return &store_apps[i];
  return NULL;
}

const StoreCategory *
store_category_by_id (const char *id)
{
  for (guint i = 0; i < store_n_categories; i++)
    if (g_str_equal (store_categories[i].id, id))
      return &store_categories[i];
  return NULL;
}
