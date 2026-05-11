# TuneList

A small macOS app for organising DJ tunes before gigs. Scans a folder of audio files, parses the filenames into Key / BPM / Artist / Title / Version, shows them in a list (and later a grid), lets you tag tracks onto emoji "boards" for quick recall, and (when a Discogs token is set) fetches cover art and metadata for the sampled tracks.

> The repo is still called `my-tunes` and the bundle identifier is still `com.makeelectronicsound.mytunes` so that previously-saved user state under `~/Library/Application Support/My Tunes/` remains the canonical store across the rename. Only the user-visible app name (window title, .app bundle, menu bar) is "TuneList".

This is **v0.1** — first version, deliberately simple. See **Current limitations** below for what's wired up and what's still scaffolded.

## Build

Requirements: macOS, Xcode command-line tools, CMake ≥ 3.22, an internet connection on first configure (to fetch JUCE).

```bash
git clone <this-repo> my-tunes
cd my-tunes
cmake -S . -B build
cmake --build build -j
```

The first `cmake -S . -B build` clones JUCE 8.0.4 into `build/_deps/` and may take a minute.

## Run

The macOS app bundle is built at:

```
build/TuneList_artefacts/TuneList.app
```

Open it the usual way:

```bash
open "build/TuneList_artefacts/TuneList.app"
```

Or double-click it in Finder.

## Opening in Projucer / Xcode

A `TuneList.jucer` Projucer file lives at the repo root. Either workflow is fine — they don't share build outputs and don't fight each other.

```bash
open TuneList.jucer       # opens in Projucer
```

In Projucer:
1. Confirm the global JUCE Modules path is set (Projucer → Preferences → "Path to JUCE modules"); the .jucer expects an Xcode-side fallback at `../../JUCE/modules` if the global path isn't set.
2. **Save Project and Open in IDE** — generates an Xcode project under `Builds/MacOSX/`.
3. Build / run from Xcode as usual; the binary is the same `TuneList.app` bundle (just under `Builds/MacOSX/build/Debug/` or `Release/` instead of `build/TuneList_artefacts/`).

The CMake setup keeps working independently — use whichever you prefer.

## Run the unit tests

A small console test runner is built alongside the app:

```bash
./build/TuneListTests_artefacts/TuneListTests
```

Currently exercises the filename parser against the example filenames from Dave's tune folder plus a handful of edge cases (commas + internal hyphen in artist, parens-heavy titles, explicit `Foo - Larse Remix` versions, malformed inputs, supported-extensions sanity).

## Cover-art lookup (Spotify-first, Discogs fallback)

After a folder scan the app fetches cover art for the sample tracks. Sources are tried in this order, per track:

1. **Spotify** — requires `SPOTIFY_CLIENT_ID` + `SPOTIFY_CLIENT_SECRET` env vars. Uses the Client Credentials flow (no browser login). If Spotify returns a track match with a usable album image, that image wins and Discogs is *not* queried for this track.
2. **Discogs** — used as fallback when Spotify has no credentials, no match, no image, or errored. Optional `DISCOGS_USER_TOKEN` env var unlocks the higher rate limit.
3. If neither produces art, the no-cover placeholder is shown.

The detail panel for each track shows an **Artwork source** row (`Spotify` / `Discogs` / `None`) and, when Spotify matched, an **Open Spotify** button that launches the track page in your browser.

The toolbar **Re-lookup** button force-refreshes every track. Use it after first granting Spotify credentials to promote old Discogs covers to Spotify ones.

### Setup

```bash
export SPOTIFY_CLIENT_ID="your_client_id_here"
export SPOTIFY_CLIENT_SECRET="your_client_secret_here"
export DISCOGS_USER_TOKEN="your_discogs_token_here"   # optional but recommended
open "build/TuneList_artefacts/TuneList.app"
```

- Get Spotify credentials from <https://developer.spotify.com/dashboard> → "Create app". You do **not** need a redirect URI for TuneList — it only does the Client Credentials flow against the public catalogue.
- Get a Discogs token at <https://www.discogs.com/settings/developers> → "Generate new token".
- **Never commit any of these.** TuneList reads all three strictly from the environment — there is no on-disk config file for them.

The app still launches fine without any of these set; it just won't fetch artwork (Spotify) or will rely on unauthenticated Discogs (rate-limited).

The Discogs User-Agent is `TuneList/0.1 +https://makeelectronicsound.com`. Requests are paced at ~1.6/sec, well under Discogs' 60/min cap.

The footer shows the combined attribution: *"Data and artwork may be provided by Spotify and Discogs. This application is not affiliated with, sponsored or endorsed by Spotify or Discogs."*

### Where artwork is cached

Both Spotify and Discogs images go through the same disk cache (content-addressed by URL hash):

```
~/Library/Application Support/My Tunes/ArtworkCache/<hash>.<ext>
```

No artwork is stored inside the repo. The Application Support data path is **deliberately kept as "My Tunes"** even though the app was renamed to TuneList — so existing user state (tags, priority, notes, cached art) migrates with no copy step.

### What the lookup does NOT do (yet)
- It only processes the **20-track sample** (Dave's deliberate cap on v0.1).
- No master vs release disambiguation (Discogs), no fuzzy matching, no per-track manual override.

## Library / state location

User state is stored at:
```
~/Library/Application Support/My Tunes/my-tunes-library.json
```

Cached cover-art thumbnails:
```
~/Library/Application Support/My Tunes/ArtworkCache/
```

(Both are created on first save / first cached lookup. Storage and cache are scaffolded but not yet wired up — see below.)

## Current limitations (v0.1)

**Sample-only mode.** To avoid hammering the Discogs API while we're getting the basics right, the app processes **at most 20 tracks** from any chosen folder. The status bar shows `Sample mode: 20 tracks   |   N loaded` to make this obvious. This will be relaxed once rate-limiting and caching are in place.

**What works in v0.1**
- Resizable Mac window with the toolbar / sidebar / list / detail / footer layout.
- Choose Folder → recursive scan → parse filenames → list view, capped at 20 tracks.
- Filename parser tolerant of commas and internal hyphens in artist names (e.g. `Mo-T`), parenthesised remix info inside titles, and explicit `Title - Version` separators.
- Five hardcoded emoji boards in the left sidebar (🙂 Rave / fun, 🌅 Sunset / chill, 🔥 Peak time, 🕺 Old school house, ⭐ Must play). Click a board to filter the list/grid; click the active board again to clear.
- List view (table) and Grid view (cards). Toggle in the toolbar.
- Right-click on a list row to toggle emoji tags via a popup menu.
- Detail panel for the selected track (filename, parsed fields, tags, Spotify + Discogs status, cover art when available, Open Spotify button when Spotify matched).
- **Spotify-first artwork lookup with Discogs fallback** — auto after scan, force-refresh via the **Re-lookup** toolbar button.
- Spotify + Discogs attribution footer in the UI.

**What's scaffolded but not wired**
- Drag-and-drop tagging of tracks onto emoji boards (right-click is the current path).
- Persistence (`TrackStorage`) — JSON load/save: tags / priority / notes / Discogs metadata are in-memory only.
- Editable priority / notes in the detail panel.

## Future work

- Plugin / VST3 / AU build — out of scope for v0.1. The core types (`TrackFile`, `TrackFilenameParser`, `TrackLibrary`, `TrackStorage`, `DiscogsClient`, `ArtworkCache`) are deliberately UI-independent so the same engine can later back a plugin without rewriting.
- Removing the 20-track sample cap once the Discogs path is rate-limited and caches survive restarts.
- Editable emoji boards (rename, change emoji, reorder).
- Music import beyond a single folder (multiple roots, watched folders).
- Audio preview / waveform.

## License

Private — not for redistribution.
