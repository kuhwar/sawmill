---
name: add-repo
description: Add an app to the Sawmill catalog from a GitHub repository link. Checks that the repository's releases contain an AppImage, then writes catalog/github.com/<owner>/<repo>/ with title, description, keywords, categories, developer, license, age rating, homepage, screenshots and a 256x256 icon. Use when the user wants to add, submit or catalog an app or GitHub repo.
argument-hint: <github-repo-link>
---

# Add a GitHub repository to the catalog

Repository: $ARGUMENTS

If no repository was given, ask for a GitHub link and stop. Only GitHub is supported by this skill;
for GitLab or Codeberg, say so and stop.

Work from the repository root. Python with Pillow is needed (`pip install -r tools/requirements.txt`),
and `rsvg-convert` (package `librsvg2-bin`) for SVG icons. Do not commit.

## 1. Check the repository ships AppImages and draft the entry

```sh
export GITHUB_TOKEN="${GITHUB_TOKEN:-$(gh auth token 2>/dev/null)}"
python3 tools/catalog.py draft "<link>"
```

The draft prints each file's content, with notes on stderr (lines starting with `#`). If it exits with
an error, stop and tell the user why in plain words: not a GitHub repository, no releases, no recent
release with a `.AppImage` file, or already in the catalog (name the existing folder).

Act on the notes:

- **archived**: the app will never update. Ask the user whether to add it anyway before continuing.
- **fork**: mention which repository it forks; the upstream may be the better entry.
- **only prereleases ship AppImages**: the entry gets a `prereleases` file; mention it.
- **other AppImages not picked**: check that the picked file is the main build. If not, write an
  `assets` override file (step 3).
- **license not detected**: find it in step 2.

## 2. Learn what the app is

Read enough to describe the app accurately. Do not invent features.

```sh
gh api repos/<owner>/<repo>/readme -H "Accept: application/vnd.github.raw" | head -150
gh api repos/<owner>/<repo>/license --jq '.license.spdx_id, .html_url'
```

When the license API says `NOASSERTION` or is vague, read the LICENSE file's first lines and pick the
exact SPDX identifier, including `-only` or `-or-later` (for example `GPL-3.0-or-later`). Use
`LicenseRef-proprietary` for closed-source apps. If it stays unclear, write `LicenseRef-unknown` and
tell the user.

## 3. Write the entry

```sh
python3 tools/catalog.py draft "<link>" --write
```

This creates `catalog/github.com/<owner>/<repo>/`. Then edit the files by these rules. Each file is
plain text ending with a newline; lists are comma-separated.

**title**: the product name as the project writes it (`LocalSend`, `Neovim`, `Xournal++`), max 40 characters.

**description**: line 1 is the summary shown on tiles: 10 to 80 characters saying what the app does
for the user. Sentence case, no final period, no app name, no "for Linux", "open-source",
"cross-platform", "free" or "simple". Good: `Share files and messages with nearby devices over your
local network`. Then a blank line, then the full text in your own words, based on the README:
one or two short paragraphs on what the app does and who it is for, a blank line, and 3 to 6 key
features as lines starting with `- `. No Markdown links, badges, images, emoji, headings or
installation instructions.

**categories**: read `catalog/categories` (`id|Name|what belongs here`) and choose the id that matches
what users do with the app, ignoring what it is built with. Add a second id only when the app clearly
belongs in both (an audio editor that also records: `create, audio`). Only ids from that file are
accepted. Examples: programmer's text editor `develop`; notes `work`; file transfer `utils`;
e-book reader `learn`; emulator or game launcher `play`.

**keywords**: 4 to 10 words people would type when they do not know the app's name. Lowercase,
a-z 0-9 and `-` `+` `.` only, no spaces. Search already covers the title, summary and categories, so
add what those miss:
- what it does and the things it handles: `notes`, `markdown`, `transfer`, `wifi`
- well-known apps it replaces or resembles: `airdrop`, `evernote`, `vim`
- file formats and protocols: `epub`, `pdf`, `webdav`, `ssh`
- never: programming languages, frameworks, platforms (`flutter`, `electron`, `rust`, `linux`,
  `android`), the app's own name, or empty words (`app`, `tool`, `free`, `open-source`)

**developer**: who makes the app, as the project presents itself. This can differ from the repository
owner: when the repository only packages someone else's app as an AppImage (for example Microsoft Edge
repackaged by a volunteer), the developer is the original maker (`Microsoft`), and you should mention
to the user that the AppImage is unofficial.

**license**: see step 2.

**age-rating**: `all`, `7+`, `12+`, `16+` or `18+`. `all` unless the app is a game, or shows
unmoderated content from the web or other people (browsers, chat, social apps: `12+`). For games use
the rating the project states, or `7+` if none. Tell the user which ratings you guessed.

**homepage**: the project's website if it has one (https only); delete the file if not. The client
falls back to the repository page.

**thumbnails**: screenshots of the app, as https image URLs (max 8). The draft takes them from the
README; open each URL and keep only real screenshots of the app (not logos, banners, diagrams or
animated demos). Delete the file if none are left.

**icon.png**: the draft picks an image from the repository and makes it 256x256. **Always look at it**
(read the PNG). It must be the app's own icon: not a wordmark with text, not another brand's logo, not
a framework's default icon, not a nightly or beta variant. If it is wrong, find the right file and set it:

```sh
gh api "repos/<owner>/<repo>/git/trees/<default-branch>?recursive=1" --jq '.tree[].path' \
  | grep -iE '\.(png|svg)$' | grep -iE 'icon|logo|<repo>'
python3 tools/catalog.py icon github.com/<owner>/<repo> <path-in-repo-or-https-url>
```

Prefer the icon the project ships for Linux (paths with `linux`, `hicolor`, `packaging`, `512x512`).
Look at the result again.

**Overrides** (only when step 4 shows the client would pick the wrong release or file):
- `assets`: one line per architecture, `x86_64 <regex>` / `aarch64 <regex>`, matched against file names
- `tag-pattern`: one regex release tags must match
- `prereleases`: `yes`

Keep regexes to plain syntax (`^ $ .+ \. (a|b) \d`); the client uses GLib's regex engine.

## 4. Verify

```sh
python3 tools/catalog.py check
python3 tools/catalog.py check --online github.com/<owner>/<repo>
python3 tools/catalog.py resolve github.com/<owner>/<repo>
```

`check` must report 0 problems. `resolve` must mark the release you expect as `LATEST`, with the main
AppImage for each architecture. If not, add an override from step 3 and run again.

## 5. Report

Tell the user, briefly:

- the folder created: `catalog/github.com/<owner>/<repo>/`,
- the latest release and the file picked for each architecture,
- the category and keywords chosen, with a few words on why,
- where the icon came from, and how many screenshots were kept,
- anything to confirm: guessed license or age rating, unofficial AppImage, archived or forked
  repository, prereleases only, overrides added.

Do not commit or open a pull request unless the user asks.
