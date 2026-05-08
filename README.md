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

## Run the unit tests

A small console test runner is built alongside the app:

```bash
./build/MyTunesTests_artefacts/MyTunesTests
```

Currently exercises the filename parser against the example filenames from Dave's tune folder plus a handful of edge cases (commas + internal hyphen in artist, parens-heavy titles, explicit `Foo - Larse Remix` versions, malformed inputs, supported-extensions sanity).

## Discogs integration

The app fetches cover art and release metadata from the [Discogs API](https://www.discogs.com/developers/) for a sampled subset of tracks. To enable, set a personal user token:

```bash
export DISCOGS_USER_TOKEN=your-token-here
open "build/MyTunes_artefacts/My Tunes.app"
```

Get a token at <https://www.discogs.com/settings/developers> → "Generate new token".

The app **runs without a token** — it just won't fetch cover art / metadata. The token must never be committed.

The app sets the User-Agent
```
MyTunes/0.1 +https://makeelectronicsound.com
```
and rate-limits requests. The footer of the window shows the required attribution: "Data provided by Discogs. This application uses Discogs' API but is not affiliated with, sponsored or endorsed by Discogs."

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
- Five hardcoded emoji boards in the left sidebar (🙂 Rave / fun, 🌅 Sunset / chill, 🔥 Peak time, 🕺 Old school house, ⭐ Must play).
- Discogs attribution footer in the UI.

**What's scaffolded but not wired**
- Grid view (toggle, tile layout, cover art).
- Drag-and-drop tagging of tracks onto emoji boards.
- Track detail panel (priority slider, notes editor, Discogs match info).
- Persistence (`TrackStorage`) — JSON load/save.
- Discogs lookups (`DiscogsClient`) — auth, rate-limit, search.
- Artwork cache (`ArtworkCache`) — content-addressed thumbnails.

## Future work

- Plugin / VST3 / AU build — out of scope for v0.1. The core types (`TrackFile`, `TrackFilenameParser`, `TrackLibrary`, `TrackStorage`, `DiscogsClient`, `ArtworkCache`) are deliberately UI-independent so the same engine can later back a plugin without rewriting.
- Removing the 20-track sample cap once the Discogs path is rate-limited and caches survive restarts.
- Editable emoji boards (rename, change emoji, reorder).
- Music import beyond a single folder (multiple roots, watched folders).
- Audio preview / waveform.

## License

Private — not for redistribution.
