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
        saved.discogsErrorReason = t.getProperty ("discogsErrorReason", juce::var()).toString();
        saved.cachedArtworkPath  = t.getProperty ("cachedArtworkPath",  juce::var()).toString();

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
        tf.discogsErrorReason = saved.discogsErrorReason;

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
        saved.discogsErrorReason = tf.discogsErrorReason;
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
        t->setProperty ("discogsErrorReason", saved.discogsErrorReason);
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
