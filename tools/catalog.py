#!/usr/bin/env python3
"""Sawmill catalog tool (CI and catalog authors only; the client never runs it).

    catalog.py check [--online] [ENTRY ...]
        Check every entry's files. With --online, also ask the forge for the given entries
        (or all) and make sure their latest release still ships an AppImage.

    catalog.py resolve <ENTRY | repository-link>
        Show the releases the client would see and which release and files it would pick.

    catalog.py draft <github-link> [--write] [--force] [--icon PATH-OR-URL]
        Check that a GitHub repository ships AppImages and draft its entry, including a 256x256 icon.

    catalog.py icon <ENTRY> [PATH-OR-URL]
        Replace only icon.png: from a file path in the app's repository, an https URL, or (without a
        source) the best guess. Always look at the result; the guess can pick the wrong image.

An ENTRY is a path inside catalog/, e.g. github.com/localsend/localsend.

Layout (the client clones this repository and reads these files directly):

    catalog/format                         "1"
    catalog/categories                     id|Name|what belongs in it   (one per line)
    catalog/featured                       "banner <entry>" or "pick <entry>" (one per line)
    catalog/<host>/<owner>/<repo>/
        title         one line, max 40 characters
        description   line 1: summary (10-80 chars, no final period); then a blank line and the full text
        keywords      comma-separated, lowercase: a-z 0-9 - + .
        categories    comma-separated, 1-2 ids from catalog/categories
        icon.png      PNG, exactly 256x256
        developer     who makes the app (may differ from the repository owner)
        license       SPDX expression, or LicenseRef-proprietary / LicenseRef-unknown
        age-rating    all | 7+ | 12+ | 16+ | 18+
        homepage      optional: https URL
        thumbnails    optional: comma-separated https image URLs
        assets        optional override: lines "<arch> <regex>" (x86_64, aarch64)
        tag-pattern   optional override: regex release tags must match
        prereleases   optional override: "yes" to allow prereleases
"""

from __future__ import annotations

import argparse
import io
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile
import urllib.error
import urllib.parse
import urllib.request
from dataclasses import dataclass
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
CATALOG = ROOT / "catalog"
FORMAT = "1"
ICON_SIZE = 256
ARCHES = ("x86_64", "aarch64")
AGE_RATINGS = ("all", "7+", "12+", "16+", "18+")

# host -> API flavour
HOSTS = {"github.com": "github", "gitlab.com": "gitlab", "codeberg.org": "gitea"}

REQUIRED = ("title", "description", "keywords", "categories", "icon.png", "developer", "license", "age-rating")
OPTIONAL = ("homepage", "thumbnails", "assets", "tag-pattern", "prereleases")

NAME_PART = re.compile(r"^[A-Za-z0-9][A-Za-z0-9._-]*$")
KEYWORD = re.compile(r"^[a-z0-9][a-z0-9+.-]*$")
SPDX = re.compile(r"^(LicenseRef-[A-Za-z0-9.-]+|[A-Za-z0-9.+-]+)( (AND|OR|WITH) [A-Za-z0-9.+-]+)*$")


# ---------------------------------------------------------------- reading files

def read_line(path: Path) -> str:
    return path.read_text(encoding="utf-8").strip()


def split_list(text: str) -> list[str]:
    return [x.strip() for x in text.split(",") if x.strip()]


def load_categories(root: Path) -> list[tuple[str, str, str]]:
    cats = []
    for line in (root / "categories").read_text(encoding="utf-8").splitlines():
        if line.strip() and not line.startswith("#"):
            cid, name, desc = (line.split("|", 2) + ["", ""])[:3]
            cats.append((cid.strip(), name.strip(), desc.strip()))
    return cats


def entry_dirs(root: Path) -> list[Path]:
    return sorted(p for host in root.iterdir() if host.is_dir() and "." in host.name
                  for owner in host.iterdir() if owner.is_dir()
                  for p in owner.iterdir() if p.is_dir())


def png_size(data: bytes) -> tuple[int, int] | None:
    if data[:8] != b"\x89PNG\r\n\x1a\n" or data[12:16] != b"IHDR":
        return None
    return int.from_bytes(data[16:20], "big"), int.from_bytes(data[20:24], "big")


@dataclass
class Overrides:
    assets: dict[str, str]
    tag_pattern: str | None
    prereleases: bool


def load_overrides(d: Path) -> Overrides:
    assets = {}
    if (d / "assets").exists():
        for line in (d / "assets").read_text(encoding="utf-8").splitlines():
            if line.strip():
                arch, _, rx = line.strip().partition(" ")
                assets[arch] = rx.strip()
    tag = read_line(d / "tag-pattern") if (d / "tag-pattern").exists() else None
    pre = (d / "prereleases").exists() and read_line(d / "prereleases") == "yes"
    return Overrides(assets, tag or None, pre)


# ---------------------------------------------------------------- checking

def check_entry(d: Path, root: Path, category_ids: set[str]) -> list[str]:
    errs: list[str] = []
    host, owner, repo = d.relative_to(root).parts
    if host not in HOSTS:
        errs.append(f"unknown host '{host}' (known: {', '.join(HOSTS)})")
    for part in (owner, repo):
        if not NAME_PART.match(part):
            errs.append(f"'{part}' is not a valid owner or repository name")

    present = {p.name for p in d.iterdir()}
    for name in REQUIRED:
        if name not in present:
            errs.append(f"missing file '{name}'")
    for name in sorted(present - set(REQUIRED) - set(OPTIONAL)):
        errs.append(f"unexpected file '{name}' (allowed: {', '.join(REQUIRED + OPTIONAL)})")

    def text(name: str) -> str | None:
        p = d / name
        if not p.is_file():
            return None
        try:
            t = p.read_text(encoding="utf-8")
        except UnicodeDecodeError:
            errs.append(f"{name}: not UTF-8 text")
            return None
        if "\r" in t:
            errs.append(f"{name}: use Unix line endings")
        return t

    def one_line(name: str, max_len: int) -> str | None:
        t = text(name)
        if t is None:
            return None
        t = t.rstrip("\n")
        if "\n" in t:
            errs.append(f"{name}: must be a single line")
        elif not t.strip() or t != t.strip():
            errs.append(f"{name}: empty, or has leading/trailing spaces")
        elif len(t) > max_len:
            errs.append(f"{name}: {len(t)} characters, max {max_len}")
        return t

    one_line("title", 40)
    one_line("developer", 60)

    if (t := text("description")) is not None:
        lines = t.rstrip("\n").split("\n")
        summary = lines[0]
        if not 10 <= len(summary) <= 80:
            errs.append(f"description: summary (line 1) is {len(summary)} characters, must be 10-80")
        if summary.endswith("."):
            errs.append("description: summary (line 1) must not end with a period")
        if len(lines) > 1 and (lines[1] != "" or not "".join(lines[2:]).strip()):
            errs.append("description: line 2 must be blank, followed by the full text")

    if (t := text("keywords")) is not None:
        kws = split_list(t)
        if not 1 <= len(kws) <= 10:
            errs.append(f"keywords: {len(kws)} given, must be 1-10")
        for k in kws:
            if not KEYWORD.match(k):
                errs.append(f"keywords: '{k}' must be lowercase a-z 0-9 - + . without spaces")
        if len(set(kws)) != len(kws):
            errs.append("keywords: duplicates")

    if (t := text("categories")) is not None:
        cats = split_list(t)
        if not 1 <= len(cats) <= 2:
            errs.append(f"categories: {len(cats)} given, must be 1 or 2")
        for c in cats:
            if c not in category_ids:
                errs.append(f"categories: '{c}' is not one of {', '.join(sorted(category_ids))}")

    if (lic := one_line("license", 120)) and not SPDX.match(lic):
        errs.append(f"license: '{lic}' is not an SPDX expression")
    if (age := one_line("age-rating", 3)) and age not in AGE_RATINGS:
        errs.append(f"age-rating: '{age}' must be one of {', '.join(AGE_RATINGS)}")
    if (hp := one_line("homepage", 300)) and not hp.startswith("https://"):
        errs.append("homepage: must start with https://")

    if (t := text("thumbnails")) is not None:
        urls = split_list(t)
        if len(urls) > 8:
            errs.append(f"thumbnails: {len(urls)} given, max 8")
        for u in urls:
            if not u.startswith("https://") or " " in u:
                errs.append(f"thumbnails: '{u}' is not an https URL")

    icon = d / "icon.png"
    if icon.is_file():
        data = icon.read_bytes()
        size = png_size(data)
        if size is None:
            errs.append("icon.png: not a PNG file")
        elif size != (ICON_SIZE, ICON_SIZE):
            errs.append(f"icon.png: {size[0]}x{size[1]}, must be {ICON_SIZE}x{ICON_SIZE}")
        if len(data) > 256 * 1024:
            errs.append(f"icon.png: {len(data) // 1024} KB, max 256 KB")

    if (t := text("assets")) is not None:
        for line in filter(str.strip, t.splitlines()):
            arch, _, rx = line.strip().partition(" ")
            if arch not in ARCHES:
                errs.append(f"assets: unknown architecture '{arch}'")
            try:
                re.compile(rx.strip())
            except re.error as e:
                errs.append(f"assets: bad regex for {arch}: {e}")
    if (tp := one_line("tag-pattern", 200)) is not None:
        try:
            re.compile(tp)
        except re.error as e:
            errs.append(f"tag-pattern: bad regex: {e}")
    if (pre := one_line("prereleases", 3)) is not None and pre != "yes":
        errs.append("prereleases: must contain 'yes' (or delete the file)")
    return errs


def check_catalog(root: Path) -> tuple[list[Path], list[str]]:
    errs: list[str] = []
    fmt = read_line(root / "format") if (root / "format").exists() else None
    if fmt != FORMAT:
        errs.append(f"format: expected '{FORMAT}', found {fmt!r}")
    cats = load_categories(root)
    ids = {c[0] for c in cats}
    for cid, name, _ in cats:
        if not re.match(r"^[a-z]+$", cid) or not name:
            errs.append(f"categories: bad line for '{cid}'")

    dirs = entry_dirs(root)
    seen: dict[str, Path] = {}
    good = []
    for d in dirs:
        rel = d.relative_to(root).as_posix()
        if rel.lower() in seen:
            errs.append(f"{rel}: same repository as {seen[rel.lower()].relative_to(root).as_posix()} (names are case-insensitive)")
        seen[rel.lower()] = d
        e = check_entry(d, root, ids)
        errs.extend(f"{rel}: {x}" for x in e)
        if not e:
            good.append(d)

    # nothing but entries 3 levels deep
    for p in root.rglob("*"):
        depth = len(p.relative_to(root).parts)
        if p.is_file() and depth not in (1, 4):
            errs.append(f"{p.relative_to(root).as_posix()}: file outside an entry folder")
        if p.is_dir() and depth == 1 and "." not in p.name:
            errs.append(f"{p.name}/: top-level folders must be hosts like github.com")

    entries = {d.relative_to(root).as_posix() for d in dirs}
    for n, line in enumerate((root / "featured").read_text(encoding="utf-8").splitlines(), 1):
        if not line.strip() or line.startswith("#"):
            continue
        kind, _, entry = line.strip().partition(" ")
        if kind not in ("banner", "pick") or entry.strip() not in entries:
            errs.append(f"featured line {n}: expected 'banner <entry>' or 'pick <entry>' for an existing entry")
    return good, errs


# ---------------------------------------------------------------- forges

def http_get(url: str, accept: str = "application/json"):
    headers = {"User-Agent": "sawmill-catalog-tool", "Accept": accept}
    host = urllib.parse.urlparse(url).hostname or ""
    if host.endswith("github.com") or host.endswith("githubusercontent.com"):
        if os.environ.get("GITHUB_TOKEN") and host == "api.github.com":
            headers["Authorization"] = f"Bearer {os.environ['GITHUB_TOKEN']}"
    try:
        with urllib.request.urlopen(urllib.request.Request(url, headers=headers), timeout=60) as r:
            return r.read()
    except urllib.error.HTTPError as e:
        raise ToolError(f"{url}: HTTP {e.code} {e.reason}") from e
    except urllib.error.URLError as e:
        raise ToolError(f"{url}: {e.reason}") from e


def http_json(url: str):
    return json.loads(http_get(url))


class ToolError(Exception):
    pass


@dataclass
class Asset:
    name: str
    url: str
    size: int | None
    digest: str | None


@dataclass
class Release:
    tag: str
    date: str
    prerelease: bool
    assets: list[Asset]


def api_base(host: str, owner: str, repo: str) -> str:
    kind = HOSTS[host]
    if kind == "github":
        return f"https://api.github.com/repos/{owner}/{repo}"
    if kind == "gitlab":
        return f"https://{host}/api/v4/projects/{urllib.parse.quote(f'{owner}/{repo}', safe='')}"
    return f"https://{host}/api/v1/repos/{owner}/{repo}"


def fetch_releases(host: str, owner: str, repo: str) -> list[Release]:
    api, kind = api_base(host, owner, repo), HOSTS[host]
    if kind == "gitlab":
        return [Release(r["tag_name"], (r.get("released_at") or "")[:10], bool(r.get("upcoming_release")),
                        [Asset(l["name"], l.get("direct_asset_url") or l["url"], None, None)
                         for l in r.get("assets", {}).get("links", [])])
                for r in http_json(f"{api}/releases?per_page=30")]
    raw = http_json(f"{api}/releases?{'per_page' if kind == 'github' else 'limit'}=30")
    return [Release(r["tag_name"], (r.get("published_at") or "")[:10], r["prerelease"],
                    [Asset(a["name"], a["browser_download_url"], a.get("size"), a.get("digest")) for a in r.get("assets", [])])
            for r in raw if not r.get("draft")]


# ---------------------------------------------------------------- picking a release (the client does the same)

VERSION_TAG = re.compile(r"^[A-Za-z_-]*\d+(\.\d+)+[a-z]?$")       # v1.2, release-5.6.2, 1.24.2b; not "nightly"
ARCH_TOKENS = (("aarch64", re.compile(r"aarch64|arm64", re.I)),
               ("x86_64", re.compile(r"x86[_-]64|amd64|x64", re.I)))
OTHER_ARCH = re.compile(r"armhf|armv7|i[3-6]86|x86[_-]32|riscv|ppc64|s390x", re.I)
NOT_MAIN_BUILD = re.compile(r"debug|dbg|symbols", re.I)


def is_appimage(name: str) -> bool:
    return name.lower().endswith(".appimage")


def asset_arch(name: str) -> str | None:
    for arch, rx in ARCH_TOKENS:
        if rx.search(name):
            return arch
    return None if OTHER_ARCH.search(name) else "x86_64"     # no architecture in the name: x86_64


def pick_files(release: Release, ov: Overrides) -> dict[str, Asset]:
    if ov.assets:
        return {arch: a for arch, rx in ov.assets.items()
                if (a := next((a for a in release.assets if re.search(rx, a.name)), None))}
    by_arch: dict[str, list[Asset]] = {}
    for a in release.assets:
        if is_appimage(a.name) and (arch := asset_arch(a.name)):
            by_arch.setdefault(arch, []).append(a)
    return {arch: min(c, key=lambda a: (bool(NOT_MAIN_BUILD.search(a.name)), len(a.name), a.name))
            for arch, c in by_arch.items()}


def usable_releases(releases: list[Release], ov: Overrides) -> list[Release]:
    rels = [r for r in releases if ov.prereleases or not r.prerelease]
    if ov.tag_pattern:
        rels = [r for r in rels if re.search(ov.tag_pattern, r.tag)]
    elif any(VERSION_TAG.match(r.tag) for r in rels):
        rels = [r for r in rels if VERSION_TAG.match(r.tag)]
    return sorted(rels, key=lambda r: r.date, reverse=True)


def pick_latest(releases: list[Release], ov: Overrides) -> tuple[Release, dict[str, Asset]] | None:
    for r in usable_releases(releases, ov):
        if files := pick_files(r, ov):
            return r, files
    return None


# ---------------------------------------------------------------- commands: check, resolve

def parse_entry(target: str) -> tuple[str, str, str]:
    m = re.match(r"^(?:https?://)?([a-z0-9.-]+\.[a-z]+)/([^/\s]+)/([^/\s#?]+?)(?:\.git)?/?(?:[#?].*)?$", target.strip())
    if not m:
        raise ToolError(f"expected a repository like github.com/owner/repo, got '{target}'")
    host = m.group(1).removeprefix("www.")
    if host not in HOSTS:
        raise ToolError(f"unknown host '{host}' (known: {', '.join(HOSTS)})")
    return host, m.group(2), m.group(3)


def cmd_check(args) -> int:
    root = Path(args.catalog)
    good, errs = check_catalog(root)
    for e in errs:
        print(f"error: {e}")
    online_failed = 0
    if args.online:
        targets = args.entries or [d.relative_to(root).as_posix() for d in good]
        for t in targets:
            d = root / t
            if not d.is_dir():
                print(f"{t}: removed, skipping")
                continue
            host, owner, repo = d.relative_to(root).parts
            try:
                picked = pick_latest(fetch_releases(host, owner, repo), load_overrides(d))
            except ToolError as e:
                picked, msg = None, str(e)
            else:
                msg = "no usable release with an AppImage among the 30 most recent"
            if picked:
                rel, files = picked
                print(f"{t}: {rel.tag} ({rel.date}) " + ", ".join(f"{a}={f.name}" for a, f in sorted(files.items())))
            else:
                print(f"error: {t}: {msg}")
                online_failed += 1
    print(f"{len(good)} valid entr{'y' if len(good) == 1 else 'ies'}, {len(errs) + online_failed} problem(s)")
    return 1 if errs or online_failed else 0


def cmd_resolve(args) -> int:
    root = Path(args.catalog)
    host, owner, repo = parse_entry(args.target)
    d = root / host / owner / repo
    ov = load_overrides(d) if d.is_dir() else Overrides({}, None, False)
    releases = fetch_releases(host, owner, repo)
    usable = {id(r) for r in usable_releases(releases, ov)}
    picked = pick_latest(releases, ov)
    print(f"https://{host}/{owner}/{repo}" + ("" if d.is_dir() else "  (not in the catalog)"))
    shown_after = 0
    for r in releases:
        is_latest = picked is not None and r is picked[0]
        if picked and r.date < picked[0].date:
            shown_after += 1
            if shown_after > 2:
                break
        files = pick_files(r, ov)
        reason = ("LATEST" if is_latest else "older" if id(r) in usable and files
                  else "skipped (prerelease)" if r.prerelease and not ov.prereleases
                  else "skipped (tag)" if id(r) not in usable else "skipped (no AppImage)")
        print(f"  {r.tag:<20} {r.date}  {reason}")
        if id(r) in usable:
            for arch, a in sorted(files.items()):
                print(f"      {arch:<8} {a.name}  {a.size or '?'} bytes  {a.digest or 'no digest'}")
    if not picked:
        print("No usable release with an AppImage among the 30 most recent.")
        return 1
    return 0


# ---------------------------------------------------------------- draft: icons and screenshots

def raw_url(owner: str, repo: str, branch: str, path: str) -> str:
    return f"https://raw.githubusercontent.com/{owner}/{repo}/{branch}/{urllib.parse.quote(path)}"


def normalise_icon(data: bytes, svg: bool) -> bytes:
    from PIL import Image
    if svg:
        if not shutil.which("rsvg-convert"):
            raise ToolError("rsvg-convert is needed for SVG icons (apt install librsvg2-bin)")
        with tempfile.NamedTemporaryFile(suffix=".svg") as f:
            f.write(data)
            f.flush()
            data = subprocess.run(["rsvg-convert", "-w", str(ICON_SIZE), "-h", str(ICON_SIZE),
                                   "--keep-aspect-ratio", f.name], check=True, capture_output=True).stdout
    img = Image.open(io.BytesIO(data)).convert("RGBA")
    bbox = img.getbbox()                                   # trim transparent margins
    if bbox:
        img = img.crop(bbox)
    side = max(img.size)
    square = Image.new("RGBA", (side, side), (0, 0, 0, 0))
    square.paste(img, ((side - img.width) // 2, (side - img.height) // 2))
    square = square.resize((ICON_SIZE, ICON_SIZE), Image.LANCZOS)
    out = io.BytesIO()
    square.save(out, "PNG", optimize=True)
    return out.getvalue()


ICON_SKIP = re.compile(r"node_modules|/tests?/|test_|screenshot|third.?party|3rdparty|vendor/|/deps/|/example|"
                       r"/fixtures?/|/docs?/(?!.*icon)|/assets/images/(?!.*(icon|logo))", re.I)
ICON_VARIANT = re.compile(r"twilight|nightly|beta|canary|insiders|preview|dev[-_]|maskable|blue|white|black|"
                          r"dark|light|outline|full|wordmark|color|colour", re.I)
ICON_BAD = re.compile(r"tray|symbolic|mono|favicon|banner|splash|social|status|badge|small|toolbar|button|"
                      r"[-_](16|22|24|32|48)\b|16x16|22x22|24x24|32x32|48x48|disabled|inactive|notification|"
                      r"background|wordmark|text|horizontal|dmg|installer|nsis|ico$", re.I)


def find_icon(owner: str, repo: str, branch: str, title: str) -> tuple[bytes, str]:
    """Best guess at the app icon: a large square PNG or SVG from the repository, else the owner's avatar."""
    try:
        tree = http_json(f"https://api.github.com/repos/{owner}/{repo}/git/trees/{branch}?recursive=1")["tree"]
    except ToolError:
        tree = []
    names = {repo.lower().replace("-", "").replace("_", ""), title.lower().replace(" ", "").replace("-", "")}
    cands = []
    for item in tree:
        path = item.get("path", "")
        low = path.lower()
        if item.get("type") != "blob" or not low.endswith((".png", ".svg")) or ICON_SKIP.search("/" + low):
            continue
        base = low.rsplit("/", 1)[-1]
        if ICON_BAD.search(base):
            continue
        score = 0
        stem = re.sub(r"[-_.]", "", base.rsplit(".", 1)[0])
        named = any(n and n in stem for n in names)
        if "icon" in base or "logo" in base or "appicon" in stem:
            score += 4
            other = re.sub(r"(app)?(icon|logo)s?|\d+x?\d*", "", stem)
            if other and not named and other not in ("app", "main", "square", "large", "big", "512", "256"):
                score -= 6                                 # someone else's logo: "zoom-logo", "amazon-logo"
        if named:
            score += 4
        if re.fullmatch(r"\d+x\d+", stem):              # icon theme sizes like 512x512.png
            score += 4
        if "hicolor" in low or "/icons/" in low or "/icon/" in low:
            score += 3
        if "linux" in low or "packaging" in low or "/branding/release" in low:
            score += 3
        if ICON_VARIANT.search(low) or "/web/" in low or low.startswith("."):
            score -= 3
        if re.search(r"(256|512|1024)", low):
            score += 2
        if low.endswith(".svg"):
            score += 1
        if score >= 4:
            cands.append((score, -low.count("/"), path))
    cands.sort(reverse=True)

    best = None
    for score, _, path in cands[:10]:
        try:
            data = http_get(raw_url(owner, repo, branch, path), accept="*/*")
        except ToolError:
            continue
        is_svg = path.lower().endswith(".svg")
        if is_svg:
            quality = (score, 4096)
        else:
            size = png_size(data)
            if not size or abs(size[0] - size[1]) > size[0] * 0.1 or min(size) < 128:
                continue
            quality = (score, min(size))
        if best is not None and quality <= best[0]:
            continue
        try:
            icon = normalise_icon(data, is_svg)
        except (subprocess.CalledProcessError, OSError, ValueError, ToolError):
            continue                                       # unreadable image: try the next candidate
        best = (quality, icon, path)
    if best:
        _, icon, path = best
        return icon, f"{path} in the repository"

    avatar = http_get(f"https://github.com/{owner}.png?size={ICON_SIZE * 2}", accept="*/*")
    return normalise_icon(avatar, False), f"the GitHub avatar of {owner} (no icon found in the repository)"


def icon_from(owner: str, repo: str, branch: str, source: str) -> tuple[bytes, str]:
    if source.startswith("https://"):
        data, where = http_get(source, accept="*/*"), source
    else:
        data, where = http_get(raw_url(owner, repo, branch, source.lstrip("/")), accept="*/*"), f"{source} in the repository"
    is_svg = source.lower().split("?")[0].endswith(".svg") or data.lstrip()[:5] in (b"<?xml", b"<svg ")
    try:
        return normalise_icon(data, is_svg), where
    except (subprocess.CalledProcessError, OSError, ValueError) as e:
        raise ToolError(f"cannot read {where} as an image: {e}") from e


def cmd_icon(args) -> int:
    root = Path(args.catalog)
    host, owner, repo = parse_entry(args.entry)
    d = root / host / owner / repo
    if not d.is_dir():
        raise ToolError(f"catalog/{host}/{owner}/{repo} does not exist")
    if host != "github.com":
        raise ToolError("icon lookup supports GitHub repositories only; pass an https URL for other hosts")
    branch = http_json(api_base(host, owner, repo)).get("default_branch", "main")
    title = read_line(d / "title") if (d / "title").exists() else repo
    icon, where = icon_from(owner, repo, branch, args.source) if args.source else find_icon(owner, repo, branch, title)
    (d / "icon.png").write_bytes(icon)
    print(f"catalog/{host}/{owner}/{repo}/icon.png from {where}")
    return 0


IMG_MD = re.compile(r"!\[([^\]]*)\]\(\s*<?([^)\s>]+)")
IMG_HTML = re.compile(r"<img\b[^>]*>", re.I)
SCREENSHOT = re.compile(r"screen|preview|demo|showcase|ui[-_.]|interface|window", re.I)


def find_thumbnails(owner: str, repo: str, branch: str) -> list[str]:
    try:
        readme = http_get(f"https://api.github.com/repos/{owner}/{repo}/readme", accept="application/vnd.github.raw").decode()
    except ToolError:
        return []
    found = [(alt, url) for alt, url in IMG_MD.findall(readme)]
    for tag in IMG_HTML.findall(readme):
        src = re.search(r'src="([^"]+)"', tag)
        alt = re.search(r'alt="([^"]*)"', tag)
        if src:
            found.append((alt.group(1) if alt else "", src.group(1)))
    urls = []
    for alt, url in found:
        if "shields.io" in url or "badge" in url.lower() or not SCREENSHOT.search(alt + " " + url):
            continue
        if not url.startswith("http"):
            url = raw_url(owner, repo, branch, url.lstrip("./"))
        url = url.replace("http://", "https://", 1)
        if re.search(r"\.(png|jpe?g|webp|gif)(\?|$)", url, re.I) or "user-images.githubusercontent.com" in url \
                or "github.com/user-attachments" in url:
            if url not in urls:
                urls.append(url)
    return urls[:4]


# ---------------------------------------------------------------- draft

TECH_TOPICS = {
    "linux", "appimage", "electron", "qt", "qt5", "qt6", "gtk", "gtk3", "gtk4", "cross-platform", "desktop",
    "desktop-app", "app", "application", "open-source", "opensource", "free", "foss", "windows", "macos",
    "android", "ios", "flutter", "flutter-apps", "dart", "javascript", "typescript", "nodejs", "node", "react",
    "react-native", "vue", "python", "python3", "rust", "go", "golang", "c", "cpp", "c-plus-plus", "java",
    "kotlin", "lua", "csharp", "dotnet", "tauri", "wails", "api", "hacktoberfest",
}
TOPIC_CATEGORY = [
    ("play", {"game", "games", "gaming", "emulator", "puzzle"}),
    ("audio", {"music", "audio", "video", "player", "podcast", "media", "streaming", "music-player", "video-player"}),
    ("develop", {"ide", "editor", "code-editor", "developer-tools", "devtools", "terminal", "git", "programming", "debugger"}),
    ("social", {"chat", "messenger", "messaging", "irc", "matrix", "xmpp", "email", "social"}),
    ("learn", {"education", "learning", "ebook", "ebook-reader", "reader", "dictionary", "science", "math"}),
    ("create", {"graphics", "drawing", "painting", "photo", "photography", "image-editor", "3d", "cad", "design", "video-editor"}),
    ("work", {"productivity", "notes", "note-taking", "office", "finance", "markdown", "todo", "pdf"}),
]


def cmd_draft(args) -> int:
    host, owner, repo = parse_entry(args.repo)
    if host != "github.com":
        raise ToolError("draft supports GitHub repositories only; write GitLab and Codeberg entries by hand")
    api = api_base(host, owner, repo)
    info = http_json(api)
    owner, repo = info["owner"]["login"], info["name"]               # canonical spelling, follows renames
    root = Path(args.catalog)
    d = root / host / owner / repo
    for other in entry_dirs(root) if root.exists() else []:
        if other.relative_to(root).as_posix().lower() == f"{host}/{owner}/{repo}".lower() and not args.force:
            raise ToolError(f"already in the catalog: catalog/{other.relative_to(root).as_posix()}")

    notes: list[str] = []
    if info.get("archived"):
        notes.append("WARNING: the repository is archived, so the app will not get updates")
    if info.get("fork"):
        notes.append(f"WARNING: the repository is a fork of {info.get('parent', {}).get('full_name', 'another repository')}")

    releases = fetch_releases(host, owner, repo)
    ov = Overrides({}, None, False)
    picked = pick_latest(releases, ov)
    if not picked and any(is_appimage(a.name) for r in releases for a in r.assets):
        ov = Overrides({}, None, True)
        picked = pick_latest(releases, ov)
        if picked:
            notes.append("WARNING: only prereleases ship AppImages; the entry gets a 'prereleases' file")
    if not picked:
        raise ToolError(f"no AppImage in the {len(releases)} most recent releases of {owner}/{repo}"
                        + ("" if releases else " (the repository has no releases)"))
    rel, files = picked
    ignored = sorted({a.name for a in rel.assets if is_appimage(a.name)} - {a.name for a in files.values()})
    if ignored:
        notes.append(f"other AppImages in {rel.tag} not picked: {', '.join(ignored)}")

    topics = [t for t in info.get("topics", []) if t not in TECH_TOPICS and t not in {owner.lower(), repo.lower()}]
    category = next((c for c, words in TOPIC_CATEGORY if words & set(topics)), "utils")
    lic = (info.get("license") or {}).get("spdx_id")
    if not lic or lic == "NOASSERTION":
        lic = "LicenseRef-unknown"
        notes.append("license not detected by GitHub; read the LICENSE file")
    gh_desc = (info.get("description") or "").strip()
    summary = gh_desc.rstrip(".")
    if not 10 <= len(summary) <= 80:
        notes.append(f"summary must be rewritten (GitHub description is {len(summary)} characters, limit 80)")
        summary = (summary[:77] + "...") if len(summary) > 80 else (summary or f"{repo} application").ljust(10, ".")
    title = repo if not repo.islower() else repo.replace("-", " ").replace("_", " ")
    owner_name = http_json(info["owner"]["url"]).get("name") or owner

    branch = info.get("default_branch", "main")
    icon, icon_source = icon_from(owner, repo, branch, args.icon) if args.icon else find_icon(owner, repo, branch, title)
    thumbs = find_thumbnails(owner, repo, branch)

    out = {
        "title": title,
        "description": f"{summary}\n\n{gh_desc or summary}\n",
        "keywords": ", ".join(topics[:8]) or repo.lower(),
        "categories": category,
        "developer": owner_name,
        "license": lic,
        "age-rating": "7+" if category == "play" else "all",
    }
    if (info.get("homepage") or "").startswith("https://"):
        out["homepage"] = info["homepage"].strip()
    if thumbs:
        out["thumbnails"] = ", ".join(thumbs)
    if ov.prereleases:
        out["prereleases"] = "yes"

    print(f"# https://{host}/{owner}/{repo}  stars={info.get('stargazers_count')}  "
          f"latest: {rel.tag} ({rel.date})  " + ", ".join(f"{a}={f.name}" for a, f in sorted(files.items())), file=sys.stderr)
    print(f"# icon from {icon_source}; {len(thumbs)} screenshot(s) found in the README", file=sys.stderr)
    print(f"# category '{category}' and keywords are guesses from GitHub topics {topics or '(none)'}", file=sys.stderr)
    for n in notes:
        print(f"# {n}", file=sys.stderr)

    if args.write:
        if d.exists() and not args.force:
            raise ToolError(f"catalog/{host}/{owner}/{repo} already exists (use --force to overwrite)")
        d.mkdir(parents=True, exist_ok=True)
        for name, value in out.items():
            (d / name).write_text(value if value.endswith("\n") else value + "\n", encoding="utf-8")
        (d / "icon.png").write_bytes(icon)
        print(d)
    else:
        for name, value in out.items():
            print(f"--- {name}\n{value.rstrip()}")
        print(f"--- icon.png\n({len(icon)} bytes, {ICON_SIZE}x{ICON_SIZE})")
    return 0


# ---------------------------------------------------------------- main

def main(argv=None) -> int:
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--catalog", default=str(CATALOG), help="catalog folder (default: %(default)s)")
    sub = p.add_subparsers(dest="cmd", required=True)
    c = sub.add_parser("check", help="check the catalog")
    c.add_argument("--online", action="store_true", help="also check that releases still ship AppImages")
    c.add_argument("entries", nargs="*", help="entries for --online (default: all)")
    r = sub.add_parser("resolve", help="show which release and files the client would pick")
    r.add_argument("target", help="an entry like github.com/owner/repo, or a repository link")
    d = sub.add_parser("draft", help="draft an entry from a GitHub repository")
    d.add_argument("repo", help="GitHub link or github.com/owner/repo")
    d.add_argument("--write", action="store_true", help="write the files into catalog/")
    d.add_argument("--force", action="store_true", help="overwrite an existing entry")
    d.add_argument("--icon", help="icon source: a file path in the repository or an https URL")
    i = sub.add_parser("icon", help="replace an entry's icon.png")
    i.add_argument("entry", help="an entry like github.com/owner/repo")
    i.add_argument("source", nargs="?", help="file path in the repository or https URL (default: best guess)")
    args = p.parse_args(argv)
    try:
        return {"check": cmd_check, "resolve": cmd_resolve, "draft": cmd_draft, "icon": cmd_icon}[args.cmd](args)
    except ToolError as e:
        print(f"error: {e}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
