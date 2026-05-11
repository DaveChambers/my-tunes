#include "TrackStorage.h"

#include "TrackLibrary.h"

namespace
{
    constexpr int kJsonFormatVersion = 1;
}

TrackStorage::TrackStorage()  = default;

TrackStorage::TrackStorage (juce::File f) : customFile (std::move (f)) {}

TrackStorage::~TrackStorage() = default;

juce::File TrackStorage::getLibraryFile()
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
               .getChildFile ("My Tunes")
               .getChildFile ("my-tunes-library.json");
}

juce::File TrackStorage::getActiveFile() const
{
    return customFile != juce::File() ? customFile : getLibraryFile();
}

juce::String TrackStorage::discogsStatusToString (TrackFile::DiscogsStatus s)
{
    using S = TrackFile::DiscogsStatus;
    switch (s)
    {
        case S::NotSearched: return "NotSearched";
        case S::Searching:   return "Searching";
        case S::Found:       return "Found";
        case S::Uncertain:   return "Uncertain";
        case S::NotFound:    return "NotFound";
        case S::Error:       return "Error";
    }
    return "NotSearched";
}

TrackFile::DiscogsStatus TrackStorage::discogsStatusFromString (const juce::String& s)
{
    using S = TrackFile::DiscogsStatus;
    if (s == "Searching") return S::Searching;
    if (s == "Found")     return S::Found;
    if (s == "Uncertain") return S::Uncertain;
    if (s == "NotFound")  return S::NotFound;
    if (s == "Error")     return S::Error;
    return S::NotSearched;
}

juce::String TrackStorage::spotifyStatusToString (TrackFile::SpotifyStatus s)
{
    using S = TrackFile::SpotifyStatus;
    switch (s)
    {
        case S::NotSearched:        return "NotSearched";
        case S::Searching:          return "Searching";
        case S::Found:              return "Found";
        case S::Uncertain:          return "Uncertain";
        case S::NotFound:           return "NotFound";
        case S::CredentialsMissing: return "CredentialsMissing";
        case S::Error:              return "Error";
    }
    return "NotSearched";
}

TrackFile::SpotifyStatus TrackStorage::spotifyStatusFromString (const juce::String& s)
{
    using S = TrackFile::SpotifyStatus;
    if (s == "Searching")          return S::Searching;
    if (s == "Found")              return S::Found;
    if (s == "Uncertain")          return S::Uncertain;
    if (s == "NotFound")           return S::NotFound;
    if (s == "CredentialsMissing") return S::CredentialsMissing;
    if (s == "Error")              return S::Error;
    return S::NotSearched;
}

juce::String TrackStorage::artworkSourceToString (TrackFile::ArtworkSource s)
{
    using A = TrackFile::ArtworkSource;
    switch (s)
    {
        case A::None:    return "None";
        case A::Spotify: return "Spotify";
        case A::Discogs: return "Discogs";
    }
    return "None";
}

TrackFile::ArtworkSource TrackStorage::artworkSourceFromString (const juce::String& s)
{
    using A = TrackFile::ArtworkSource;
    if (s == "Spotify") return A::Spotify;
    if (s == "Discogs") return A::Discogs;
    return A::None;
}

bool TrackStorage::load()
{
    byPath.clear();
    savedFolderPath.clear();

    const auto file = getActiveFile();
    if (! file.existsAsFile())
        return false;

    const auto text = file.loadFileAsString();
    const auto json = juce::JSON::parse (text);
    if (! json.isObject())
        return false;

    savedFolderPath = json.getProperty ("folder", juce::var()).toString();

    const auto tracks = json.getProperty ("tracks", juce::var());
    if (! tracks.isArray())
        return true; // valid file, just no tracks yet

    for (int i = 0; i < tracks.size(); ++i)
    {
        const auto& t = tracks[i];
        if (! t.isObject()) continue;

        const auto path = t.getProperty ("path", juce::var()).toString();
        if (path.isEmpty()) continue;

        SavedTrack saved;

        const auto tagsVar = t.getProperty ("tags", juce::var());
        if (tagsVar.isArray())
            for (int k = 0; k < tagsVar.size(); ++k)
                saved.emojiTags.add (tagsVar[k].toString());

        saved.priority = (int) t.getProperty ("priority", 0);
        saved.notes    = t.getProperty ("notes", juce::var()).toString();

        saved.discogsStatus      = discogsStatusFromString (
                                       t.getProperty ("discogsStatus", "NotSearched").toString());
        saved.discogsArtist      = t.getProperty ("discogsArtist",      juce::var()).toString();
        saved.discogsTitle       = t.getProperty ("discogsTitle",       juce::var()).toString();
        saved.discogsReleaseId   = t.getProperty ("discogsReleaseId",   juce::var()).toString();
        saved.discogsCoverUrl    = t.getProperty ("discogsCoverUrl",    juce::var()).toString();
        saved.discogsThumbUrl    = t.getProperty ("discogsThumbUrl",    juce::var()).toString();
        saved.discogsExternalUrl = t.getProperty ("discogsExternalUrl", juce::var()).toString();
        saved.discogsErrorReason = t.getProperty ("discogsErrorReason", juce::var()).toString();

        // Spotify fields — tolerant of older JSON without these keys.
        saved.spotifyStatus      = spotifyStatusFromString (
                                       t.getProperty ("spotifyStatus", "NotSearched").toString());
        saved.spotifyArtist      = t.getProperty ("spotifyArtist",      juce::var()).toString();
        saved.spotifyTitle       = t.getProperty ("spotifyTitle",       juce::var()).toString();
        saved.spotifyTrackId     = t.getProperty ("spotifyTrackId",     juce::var()).toString();
        saved.spotifyAlbumId     = t.getProperty ("spotifyAlbumId",     juce::var()).toString();
        saved.spotifyExternalUrl = t.getProperty ("spotifyExternalUrl", juce::var()).toString();
        saved.spotifyImageUrl    = t.getProperty ("spotifyImageUrl",    juce::var()).toString();
        saved.spotifyErrorReason = t.getProperty ("spotifyErrorReason", juce::var()).toString();

        saved.artworkSource      = artworkSourceFromString (
                                       t.getProperty ("artworkSource", "None").toString());
        saved.cachedArtworkPath  = t.getProperty ("cachedArtworkPath",  juce::var()).toString();

        // Back-compat: older files only had Discogs metadata. If the
        // saved track has no artworkSource key but has cached art and a
        // Found Discogs status, treat it as Discogs-sourced so the
        // skip-cache path in LookupController still works on first
        // load after upgrade.
        if (saved.artworkSource == TrackFile::ArtworkSource::None
            && saved.cachedArtworkPath.isNotEmpty()
            && saved.discogsStatus == TrackFile::DiscogsStatus::Found)
        {
            saved.artworkSource = TrackFile::ArtworkSource::Discogs;
        }

        byPath.emplace (path, std::move (saved));
    }

    return true;
}

void TrackStorage::applySavedFieldsToLibrary (TrackLibrary& library) const
{
    const int n = library.getNumTracks();
    for (int i = 0; i < n; ++i)
    {
        auto& tf = library.getTrack (i);
        const auto path = tf.file.getFullPathName();
        if (path.isEmpty())
            continue;

        const auto it = byPath.find (path);
        if (it == byPath.end())
            continue;

        const auto& saved = it->second;

        tf.emojiTags = saved.emojiTags;
        tf.priority  = saved.priority;
        tf.notes     = saved.notes;

        tf.discogsStatus      = saved.discogsStatus;
        tf.discogsArtist      = saved.discogsArtist;
        tf.discogsTitle       = saved.discogsTitle;
        tf.discogsReleaseId   = saved.discogsReleaseId;
        tf.discogsCoverUrl    = saved.discogsCoverUrl;
        tf.discogsExternalUrl = saved.discogsExternalUrl;
        tf.discogsErrorReason = saved.discogsErrorReason;

        tf.spotifyStatus      = saved.spotifyStatus;
        tf.spotifyArtist      = saved.spotifyArtist;
        tf.spotifyTitle       = saved.spotifyTitle;
        tf.spotifyTrackId     = saved.spotifyTrackId;
        tf.spotifyAlbumId     = saved.spotifyAlbumId;
        tf.spotifyExternalUrl = saved.spotifyExternalUrl;
        tf.spotifyImageUrl    = saved.spotifyImageUrl;
        tf.spotifyErrorReason = saved.spotifyErrorReason;

        tf.artworkSource      = saved.artworkSource;

        if (saved.cachedArtworkPath.isNotEmpty())
        {
            const juce::File cached (saved.cachedArtworkPath);
            if (cached.existsAsFile())
                tf.cachedArtworkPath = cached;
        }
    }
}

bool TrackStorage::snapshotAndSave (const TrackLibrary& library)
{
    // Refresh in-memory map from current library state.
    byPath.clear();
    savedFolderPath = library.getFolder().getFullPathName();

    auto* root  = new juce::DynamicObject();
    auto* arr   = new juce::DynamicObject(); // unused — array goes raw
    juce::Array<juce::var> tracksArr;

    const int n = library.getNumTracks();
    for (int i = 0; i < n; ++i)
    {
        const auto& tf = library.getTrack (i);
        const auto  path = tf.file.getFullPathName();
        if (path.isEmpty()) continue;

        SavedTrack saved;
        saved.emojiTags          = tf.emojiTags;
        saved.priority           = tf.priority;
        saved.notes              = tf.notes;
        saved.discogsStatus      = tf.discogsStatus;
        saved.discogsArtist      = tf.discogsArtist;
        saved.discogsTitle       = tf.discogsTitle;
        saved.discogsReleaseId   = tf.discogsReleaseId;
        saved.discogsCoverUrl    = tf.discogsCoverUrl;
        saved.discogsExternalUrl = tf.discogsExternalUrl;
        saved.discogsErrorReason = tf.discogsErrorReason;
        saved.spotifyStatus      = tf.spotifyStatus;
        saved.spotifyArtist      = tf.spotifyArtist;
        saved.spotifyTitle       = tf.spotifyTitle;
        saved.spotifyTrackId     = tf.spotifyTrackId;
        saved.spotifyAlbumId     = tf.spotifyAlbumId;
        saved.spotifyExternalUrl = tf.spotifyExternalUrl;
        saved.spotifyImageUrl    = tf.spotifyImageUrl;
        saved.spotifyErrorReason = tf.spotifyErrorReason;
        saved.artworkSource      = tf.artworkSource;
        saved.cachedArtworkPath  = tf.cachedArtworkPath.getFullPathName();
        byPath[path] = saved;

        auto* t = new juce::DynamicObject();
        t->setProperty ("path",     path);
        t->setProperty ("filename", tf.filename);

        juce::Array<juce::var> tagsArr;
        for (const auto& tag : saved.emojiTags) tagsArr.add (tag);
        t->setProperty ("tags",     tagsArr);
        t->setProperty ("priority", saved.priority);
        t->setProperty ("notes",    saved.notes);

        t->setProperty ("discogsStatus",      discogsStatusToString (saved.discogsStatus));
        t->setProperty ("discogsArtist",      saved.discogsArtist);
        t->setProperty ("discogsTitle",       saved.discogsTitle);
        t->setProperty ("discogsReleaseId",   saved.discogsReleaseId);
        t->setProperty ("discogsCoverUrl",    saved.discogsCoverUrl);
        t->setProperty ("discogsThumbUrl",    saved.discogsThumbUrl);
        t->setProperty ("discogsExternalUrl", saved.discogsExternalUrl);
        t->setProperty ("discogsErrorReason", saved.discogsErrorReason);

        t->setProperty ("spotifyStatus",      spotifyStatusToString (saved.spotifyStatus));
        t->setProperty ("spotifyArtist",      saved.spotifyArtist);
        t->setProperty ("spotifyTitle",       saved.spotifyTitle);
        t->setProperty ("spotifyTrackId",     saved.spotifyTrackId);
        t->setProperty ("spotifyAlbumId",     saved.spotifyAlbumId);
        t->setProperty ("spotifyExternalUrl", saved.spotifyExternalUrl);
        t->setProperty ("spotifyImageUrl",    saved.spotifyImageUrl);
        t->setProperty ("spotifyErrorReason", saved.spotifyErrorReason);

        t->setProperty ("artworkSource",      artworkSourceToString (saved.artworkSource));
        t->setProperty ("cachedArtworkPath",  saved.cachedArtworkPath);

        tracksArr.add (juce::var (t));
    }
    delete arr; // unused placeholder

    root->setProperty ("version", kJsonFormatVersion);
    root->setProperty ("folder",  savedFolderPath);
    root->setProperty ("tracks",  tracksArr);

    const auto json = juce::JSON::toString (juce::var (root));

    const auto file = getActiveFile();
    file.getParentDirectory().createDirectory();

    juce::TemporaryFile tmp (file);
    if (! tmp.getFile().replaceWithText (json))
        return false;

    return tmp.overwriteTargetFileWithTemporary();
}
