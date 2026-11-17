# Sawmill

An app store for AppImages published as releases on GitHub, GitLab, Codeberg and other Gitea servers.

```
this repository ──(git clone / git fetch)──► ~/.cache/sawmill/catalog/ ──► Sawmill client
catalog/github.com/<owner>/<repo>/           local copy, searched          asks each installed app's
plain-text files + icon.png                  on the user's machine         repository for new releases
```

There is no server and no build step. The client keeps a shallow, sparse clone of this repository that
contains only `catalog/`, reads the files directly and searches them locally. The catalog says **where**
each app's releases are and describes the app; it never stores versions.

| Folder | What it is |
|---|---|
| `client/` | The desktop app, in C with GTK 4 and libadwaita |
| `catalog/` | One folder per app, named after its repository: `catalog/github.com/<owner>/<repo>/` |
| `tools/` | `catalog.py`: checks entries, drafts new ones, picks icons, previews release lookups (CI and authors only) |
| `.github/workflows/` | CI: build the client; check the catalog on every push to `main` and every pull request |

## Catalog layout

```
catalog/
├── format                 "1". A client that finds a newer number asks the user to update Sawmill.
├── categories             id|Name|what belongs here   (one per line; entries may only use these ids)
├── featured               "banner <entry>" or "pick <entry>"   (one per line)
└── github.com/localsend/localsend/
    ├── title              LocalSend
    ├── description        line 1: summary for tiles (10-80 chars); blank line; full text
    ├── keywords           airdrop, file-transfer, share, send, nearby, wifi, lan
    ├── categories         utils                     (1-2 ids from catalog/categories)
    ├── icon.png           256x256 PNG
    ├── developer          LocalSend                 (may differ from the repository owner)
    ├── license            Apache-2.0                (SPDX)
    ├── age-rating         all | 7+ | 12+ | 16+ | 18+
    ├── homepage           optional, https
    ├── thumbnails         optional, comma-separated https screenshot URLs
    └── assets, tag-pattern, prereleases    optional overrides, only when the automatic pick is wrong
```

The folder path is the app's ID (`github.com/localsend/localsend`) and tells the client where releases
live. Other hosts are other top-level folders: `gitlab.com/...`, `codeberg.org/...`.

All files are UTF-8 text ending with a newline, except `icon.png`. CI checks every entry on each push to
`main` and each pull request: required files present and no unknown ones, single-line fields, summary
length, keyword characters, categories only from the list, valid SPDX license and age rating, a
256x256 PNG icon, valid override regexes, and, for changed entries, that the latest release still
ships an AppImage.

## Adding an app

In Claude Code, `/add-repo <github-link>` checks that the repository's releases contain an AppImage and
writes the whole entry, including the icon (see `.claude/skills/add-repo/SKILL.md`). By hand:

```sh
sudo apt install python3-pil librsvg2-bin       # or: pip install -r tools/requirements.txt
python3 tools/catalog.py draft <github-link> --write          # draft every file, icon included
python3 tools/catalog.py icon github.com/<owner>/<repo> [path-in-repo|https-url]   # replace the icon
python3 tools/catalog.py check [--online [entry ...]]         # what CI runs
python3 tools/catalog.py resolve github.com/<owner>/<repo>    # which release and file the client picks
```

Always look at the drafted `icon.png`; the automatic choice can pick a wordmark or another brand's logo.
The catalog holds 20 apps. Python is only used by CI and catalog authors, never by the client.

## Client

```sh
sudo apt install libgtk-4-dev libadwaita-1-dev libsoup-3.0-dev libjson-glib-dev meson ninja-build
meson setup client/build client          # -Dcatalog_url=<git URL> to use another catalog repository
ninja -C client/build
./client/build/sawmill
```

At run time the client needs `git`, and `webp-pixbuf-loader` to show WebP screenshots.

On start it shows the catalog copy it already has and updates it in the background with `git`
(menu: Refresh Catalog, or Ctrl+R). The first run shows a loading screen while it clones; if that fails,
it shows the reason and a Try Again button. Screenshots are downloaded when an app's page opens and kept
in `~/.cache/sawmill/images`.

### Installing apps

Install looks up the latest release (rules below), downloads the AppImage, checks its sha256 when the
release publishes one, and adds a launcher to the system's app grid. For `github.com/localsend/localsend`:

```
~/.local/share/sawmill/github.com/localsend/localsend/app.AppImage    the app
~/.local/share/sawmill/github.com/localsend/localsend/icon.png        icon from inside the AppImage
~/.local/share/sawmill/github.com/localsend/localsend/version         installed release tag
~/.local/share/applications/sawmill-github.com-localsend-localsend.desktop
```

The launcher is the AppImage's own `.desktop` file (translations, categories, MIME types and actions
kept), with `Exec`, `TryExec` and `Icon` pointing at the installed files. The `.desktop` file and icon are
unpacked with the AppImage's `--appimage-extract`, so the AppImage's runtime runs briefly during install;
the app itself does not. If the AppImage has no icon, the catalog's `icon.png` is used. Uninstall removes
the launcher and the app's folder. A cancelled or failed first install leaves nothing behind.

Running AppImages needs FUSE (`fusermount`, and `libfuse2` for older AppImages). Checking for updates is
not implemented yet.

To test the installer without the UI (uses the network; set `XDG_DATA_HOME` to keep it away from your
real `~/.local/share`):

```sh
XDG_DATA_HOME=/tmp/sawmill-test ./client/build/install-test catalog github.com/neovim/neovim [--keep]
```

| Variable | Use |
|---|---|
| `SAWMILL_CATALOG_DIR=catalog` | read a local catalog folder directly, no git (for catalog work) |
| `SAWMILL_CATALOG_URL=<git URL>` | clone from another repository, e.g. `file:///path/to/sawmill` |
| `SAWMILL_OPEN=<entry\|category\|installed\|updates\|search:words>` | open a page at start |
| `SAWMILL_SCREENSHOT=out.png` | save a PNG of the window after 1.5 s and quit |

Code: `store-catalog.c` reads the catalog and ranks search results, `store-sync.c` runs git,
`store-images.c` downloads and caches screenshots, `store-install.c` installs, uninstalls and launches
AppImages, `store-window.c` is the UI, `store-art.c` draws category glyphs.

### Catalog copy

```sh
# first run
git clone --depth 1 --filter=blob:none --sparse https://github.com/<owner>/sawmill ~/.cache/sawmill
git -C ~/.cache/sawmill sparse-checkout set catalog
# every update: always reset, so local edits or conflicts never block it
git -C ~/.cache/sawmill fetch --depth 1 origin main
git -C ~/.cache/sawmill reset --hard FETCH_HEAD
```

- `~/.cache` (`$XDG_CACHE_HOME`) can be cleared at any time; the client clones again when the copy is
  missing. The list of installed apps lives in `~/.local/share/sawmill`, never in the cache.
- The client reads every entry into memory after each update and searches there as you type; it skips
  entries it cannot read and keeps the rest.
- `git fetch` does not use GitHub's API quota, so catalog updates never compete with update checks.

### Picking the latest release

`tools/catalog.py` (`pick_latest`) is the reference; the client does the same. For each installed app,
every few hours:

1. Fetch the 30 most recent releases: GitHub `https://api.github.com/repos/<owner>/<repo>/releases?per_page=30`,
   GitLab `https://<host>/api/v4/projects/<owner%2Frepo>/releases?per_page=30`,
   Gitea/Codeberg `https://<host>/api/v1/repos/<owner>/<repo>/releases?limit=30`.
2. Drop drafts, and prereleases unless the entry has `prereleases`.
3. With `tag-pattern`, keep matching tags. Otherwise, if any tag looks like a version
   (`v1.2`, `release-5.6.2`, `1.24.2b`), drop tags that don't (`nightly`, `stable`, `continuous`).
4. Take the newest remaining release with an AppImage for this computer's architecture. Without an
   `assets` override, an AppImage's architecture comes from its name (`aarch64`/`arm64`, `x86_64`/`amd64`/`x64`,
   none means x86_64; `armhf`, `i686` and others are ignored). If there are several, prefer one without
   `debug`/`dbg`/`symbols`, then the shortest name.
5. If its tag differs from the installed one, offer the update. Download, and verify the sha256 from
   GitHub's asset `digest` field when present, or from a `SHA256SUMS` file in the release.
6. If nothing in the 30 is usable (projects with many nightly builds) and prereleases are not allowed,
   ask GitHub's `/releases/latest`.

All 20 current entries are picked correctly by these rules, without overrides.

Unauthenticated GitHub API access allows 60 requests per hour per IP address. The client checks only
installed apps, spreads checks out, backs off when the limit is reached, and can use an optional
personal token.

Because every user's client reads `main` directly, protect it: require pull-request reviews or limit who
can push, and consider signed commits that the client can check with `git verify-commit`.
