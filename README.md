# My Tunes

A small macOS app for organising DJ tunes before gigs. Scans a folder of audio files, parses the filenames into Key / BPM / Artist / Title / Version, shows them in a list (and later a grid), lets you tag tracks onto emoji "boards" for quick recall, and (when a Discogs token is set) fetches cover art and metadata for the sampled tracks.

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
build/MyTunes_artefacts/My Tunes.app
```

Open it the usual way:

```bash
open "build/MyTunes_artefacts/My Tunes.app"
```

Or double-click it in Finder.

## Opening in Projucer / Xcode

A `MyTunes.jucer` Projucer file lives at the repo root. Either workflow is fine — they don't share build outputs and don't fight each other.

```bash
open MyTunes.jucer       # opens in Projucer
```

In Projucer:
1. Confirm the global JUCE Modules path is set (Projucer → Preferences → "Path to JUCE modules"); the .jucer expects an Xcode-side fallback at `../../JUCE/modules` if the global path isn't set.
2. **Save Project and Open in IDE** — generates an Xcode project under `Builds/MacOSX/`.
3. Build / run from Xcode as usual; the binary is the same `My Tunes.app` bundle (just under `Builds/MacOSX/build/Debug/` or `Release/` instead of `build/MyTunes_artefacts/`).

The CMake setup keeps working independently — use whichever you prefer.

## Run the unit tests

A small console test runner is built alongside the app:

```bash
./build/MyTunesTests_artefacts/MyTunesTests
```

Currently exercises the filename parser against the example filenames from Dave's tune folder plus a handful of edge cases (commas + internal hyphen in artist, parens-heavy titles, explicit `Foo - Larse Remix` versions, malformed inputs, supported-extensions sanity).

## Discogs cover lookup

Click the **Lookup Covers** button in the toolbar to fetch cover art and basic release metadata from the [Discogs API](https://www.discogs.com/developers/) for the loaded tracks. The lookup runs on a background thread; the toolbar status updates as it goes (`Looking up 3/20`).

A user token is **optional but recommended** — without one, Discogs is more aggressively rate-limited.

To set a token before launching the app:

```bash
export DISCOGS_USER_TOKEN="your-token-here"
open "build/MyTunes_artefacts/My Tunes.app"
```

Get a token at <https://www.discogs.com/settings/developers> → "Generate new token". **Never commit it.** The app reads it from the environment, never from a tracked file.

The app sets the User-Agent
```
MyTunes/0.1 +https://makeelectronicsound.com
```
and paces requests at roughly 1.6/sec, well under Discogs' 60/min cap. The footer of the window shows the required attribution: *"Data provided by Discogs. This application uses Discogs' API but is not affiliated with, sponsored or endorsed by Discogs."*

### What the lookup does
- Searches by `artist` + `track` against `database/search`
- Picks the first result that carries a thumbnail when possible
- Marks the track **Found** if Discogs' result string contains the input artist, **Uncertain** otherwise
- Downloads the thumbnail and caches it under `~/Library/Application Support/My Tunes/ArtworkCache/`
- The grid view paints the cached thumbnail; the detail panel shows a larger version of the same image

### What the lookup does NOT do (yet)
- It only processes the **20-track sample** (Dave's deliberate cap on v0.1).
- It doesn't yet skip tracks already in `Found` state across app restarts — Discogs metadata is in-memory only, so a fresh app launch starts everyone back at `Not searched`. Cached image files on disk persist; once `TrackStorage` is wired, the metadata side will too.
- No master vs release disambiguation, no fuzzy matching, no per-track manual override.

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
- Detail panel for the selected track (filename, parsed fields, tags, Discogs status, cover art when available).
- Discogs lookup via the **Lookup Covers** button — rate-limited, optional token, cached thumbnails.
- Discogs attribution footer in the UI.

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
