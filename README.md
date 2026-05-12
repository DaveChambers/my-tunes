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

## Cover-art lookup

Artwork lookup goes through the **TuneList metadata service** — a small Cloud Run proxy that holds the Spotify and Discogs API credentials in Google Secret Manager and implements the Spotify-first / Discogs-fallback policy server-side. The desktop app just calls the proxy:

```
GET https://tunelist-metadata-96674523622.europe-north1.run.app/lookup
    ?artist=<artist>&title=<title>
```

After a folder scan the app issues one request per track. The proxy returns a normalised JSON payload telling TuneList which source won (Spotify, Discogs, or none) and the image URL to cache.

**End users do not need Spotify or Discogs API tokens.** The macOS app no longer reads `SPOTIFY_CLIENT_ID`, `SPOTIFY_CLIENT_SECRET`, or `DISCOGS_USER_TOKEN`; those credentials live in the Cloud Run service.

The detail panel for each track shows an **Artwork source** row (`Spotify` / `Discogs` / `None`), the matched artist / title / album, the exact `Lookup query` the proxy ran, and **Open Spotify** / **Open Discogs** buttons that launch the corresponding web page in your browser when the proxy supplied a URL.

The toolbar **Re-lookup** button force-refreshes every track via the proxy. Use it after switching folders or after the proxy itself gets a backend tweak.

### Where artwork is cached

Both Spotify and Discogs images go through the same on-disk cache (content-addressed by URL hash):

```
~/Library/Application Support/My Tunes/ArtworkCache/<hash>.<ext>
```

No artwork is stored inside the repo. The Application Support data path is **deliberately kept as "My Tunes"** even though the app was renamed to TuneList — so existing user state (tags, priority, notes, cached art) migrates with no copy step.

The footer shows the combined attribution: *"Data and artwork may be provided by Spotify and Discogs. This application is not affiliated with, sponsored or endorsed by Spotify or Discogs."*

### Developer note: legacy direct-API clients

The repository still contains `SpotifyClient` and `DiscogsClient` source files plus a `TuneListConfig` credential loader. These are no longer wired into the running app — `LookupController` exclusively uses `MetadataProxyClient`. The files are kept compiled (so the unit tests covering URL-encoding, credential resolution, and JSON parsing remain green) but the bundled app will never read `SPOTIFY_CLIENT_ID` / `SPOTIFY_CLIENT_SECRET` / `DISCOGS_USER_TOKEN` during normal use. Drop them entirely whenever the team is comfortable.

### What the lookup does NOT do (yet)
- No fuzzy matching beyond the desktop's token-overlap scoring, and no per-track manual override.
- Lookup is paced — one request at a time, with a small inter-request wait — so very large folders fill in artwork progressively rather than all at once.

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

**What works in v0.1**
- Resizable Mac window with the toolbar / sidebar / list / detail / footer layout.
- Choose Folder → recursive scan → parse filenames → list view loads every supported audio file (no sample cap).
- Filename parser tolerant of commas and internal hyphens in artist names (e.g. `Mo-T`), parenthesised remix info inside titles, and explicit `Title - Version` separators.
- Five hardcoded emoji boards in the left sidebar (🙂 Rave / fun, 🌅 Sunset / chill, 🔥 Peak time, 🕺 Old school house, ⭐ Must play). Click a board to filter the list/grid; click the active board again to clear.
- List view (table) and Grid view (cards). Toggle in the toolbar.
- Right-click on a list row to toggle emoji tags via a popup menu.
- Detail panel for the selected track (filename, parsed fields, tags, Spotify + Discogs status, cover art when available, Open Spotify button when Spotify matched).
- **Spotify-first artwork lookup with Discogs fallback via the TuneList metadata service** — auto after scan, force-refresh via the **Re-lookup** toolbar button.
- Spotify + Discogs attribution footer in the UI.

**What's scaffolded but not wired**
- Drag-and-drop tagging of tracks onto emoji boards (right-click is the current path).
- Persistence (`TrackStorage`) — JSON load/save: tags / priority / notes / Discogs metadata are in-memory only.
- Editable priority / notes in the detail panel.

## Future work

- Plugin / VST3 / AU build — out of scope for v0.1. The core types (`TrackFile`, `TrackFilenameParser`, `TrackLibrary`, `TrackStorage`, `DiscogsClient`, `ArtworkCache`) are deliberately UI-independent so the same engine can later back a plugin without rewriting.
- Background folder scan (current scan runs on the message thread; fine for thousands of files, but a giant SMB-mounted folder will briefly block the UI while filenames are parsed).
- Editable emoji boards (rename, change emoji, reorder).
- Music import beyond a single folder (multiple roots, watched folders).
- Audio preview / waveform.

## License

Private — not for redistribution.
