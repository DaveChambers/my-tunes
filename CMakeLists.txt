cmake_minimum_required(VERSION 3.22)
project(TuneList VERSION 0.1.0 LANGUAGES C CXX OBJC OBJCXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Fetch JUCE. Pinned to 8.0.4 — works on macOS 15+ SDK and is a stable
# point we already use in sibling projects.
include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.4
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(JUCE)

# --- macOS standalone app --------------------------------------------------

# Bundle icon: we ship a pre-built multi-resolution .icns
# (Assets/TuneListIcon.icns, built once with iconutil from the source
# PNG and committed to the repo) instead of letting JUCE bake a
# single-size Icon.icns from the PNG. JUCE's juceaide produces an
# .icns containing only the size derived from the input PNG, which
# leaves Finder/Dock to scale at small sizes and sometimes falls back
# to a generic-looking glyph. Shipping the standard 10-size iconset
# gives macOS exactly what it wants at every display size.
#
# We give the icon a non-generic filename (TuneListIcon.icns rather
# than the JUCE default Icon.icns) so Launch Services treats the
# bundle's icon resource as new and refreshes its cached glyph for
# CFBundleIdentifier=com.makeelectronicsound.mytunes — which is the
# main reason a previously-cached default icon was still appearing
# in Dock/Cmd+Tab even though the .icns was valid.
#
# BUNDLE_ID is intentionally kept on the legacy "mytunes" reverse-DNS
# so that previously-saved user state under
#   ~/Library/Application Support/My Tunes/
# remains the canonical store across the rename to TuneList.
juce_add_gui_app(TuneList
    PRODUCT_NAME    "TuneList"
    BUNDLE_ID       "com.makeelectronicsound.mytunes"
    COMPANY_NAME    "Make Electronic Sound"
    COMPANY_WEBSITE "https://makeelectronicsound.com"
)

target_sources(TuneList PRIVATE
    Source/Main.cpp
    Source/MainComponent.cpp
    Source/MainComponent.h
    Source/TrackFile.h
    Source/TrackFilenameParser.cpp
    Source/TrackFilenameParser.h
    Source/TrackLibrary.cpp
    Source/TrackLibrary.h
    Source/TrackStorage.cpp
    Source/TrackStorage.h
    Source/TuneListConfig.cpp
    Source/TuneListConfig.h
    Source/DiscogsClient.cpp
    Source/DiscogsClient.h
    Source/SpotifyClient.cpp
    Source/SpotifyClient.h
    Source/MetadataProxyClient.cpp
    Source/MetadataProxyClient.h
    Source/ArtworkCache.cpp
    Source/ArtworkCache.h
    Source/LookupController.cpp
    Source/LookupController.h
    Source/LookupMatching.cpp
    Source/LookupMatching.h
    Source/TrackPreviewPlayer.cpp
    Source/TrackPreviewPlayer.h
    Source/MiniPlayerComponent.cpp
    Source/MiniPlayerComponent.h
    Source/TrackListComponent.cpp
    Source/TrackListComponent.h
    Source/TrackGridComponent.cpp
    Source/TrackGridComponent.h
    Source/EmojiTagBoardComponent.cpp
    Source/EmojiTagBoardComponent.h
    Source/TrackDetailComponent.cpp
    Source/TrackDetailComponent.h
)

target_compile_definitions(TuneList PRIVATE
    JUCE_USE_CURL=0
    JUCE_WEB_BROWSER=0
    JUCE_STANDALONE_APPLICATION=1
    JUCE_USE_MP3AUDIOFORMAT=1
    DONT_SET_USING_JUCE_NAMESPACE=1
    MYTUNES_VERSION_STRING="${PROJECT_VERSION}"
)

target_link_libraries(TuneList PRIVATE
    juce::juce_audio_basics
    juce::juce_audio_devices
    juce::juce_audio_formats
    juce::juce_core
    juce::juce_data_structures
    juce::juce_events
    juce::juce_graphics
    juce::juce_gui_basics
    juce::juce_gui_extra
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags
)

# Post-build: install our pre-built multi-resolution .icns into the app
# bundle and point CFBundleIconFile at it. JUCE has already written
# Info.plist by this stage; we patch the icon-file key in place with
# PlistBuddy. We also remove the JUCE-generated Icon.icns to avoid two
# competing icon resources in Contents/Resources.
set(_tunelist_icns "${CMAKE_CURRENT_SOURCE_DIR}/Assets/TuneListIcon.icns")
set(_lsregister
    "/System/Library/Frameworks/CoreServices.framework/Frameworks/LaunchServices.framework/Support/lsregister")
if(APPLE AND EXISTS "${_tunelist_icns}")
    add_custom_command(TARGET TuneList POST_BUILD
        COMMENT "Install TuneListIcon.icns into bundle + patch Info.plist + lsregister"
        # Replace the JUCE-baked Icon.icns with our multi-size one.
        COMMAND ${CMAKE_COMMAND} -E copy "${_tunelist_icns}"
                "$<TARGET_BUNDLE_DIR:TuneList>/Contents/Resources/TuneListIcon.icns"
        COMMAND ${CMAKE_COMMAND} -E remove -f
                "$<TARGET_BUNDLE_DIR:TuneList>/Contents/Resources/Icon.icns"
        # Point Info.plist at the new name. JUCE writes the key for us
        # when ICON_BIG is passed; we no longer pass it, so use Add
        # (which is silently a no-op if the key already exists from a
        # previous cached configure) and follow with Set to overwrite.
        COMMAND /usr/libexec/PlistBuddy
                -c "Add :CFBundleIconFile string TuneListIcon"
                "$<TARGET_BUNDLE_DIR:TuneList>/Contents/Info.plist" || true
        COMMAND /usr/libexec/PlistBuddy
                -c "Set :CFBundleIconFile TuneListIcon"
                "$<TARGET_BUNDLE_DIR:TuneList>/Contents/Info.plist"
        # Touch the bundle and re-register it with Launch Services so
        # Dock / Cmd+Tab / Finder pick up the new icon without the user
        # having to log out. -f forces a fresh registration even if the
        # bundle was already known. Errors are tolerated because some
        # macOS versions ship lsregister at a slightly different path.
        COMMAND touch "$<TARGET_BUNDLE_DIR:TuneList>"
        COMMAND ${_lsregister} -f "$<TARGET_BUNDLE_DIR:TuneList>" || true
        VERBATIM)
endif()

# --- Console test runner ---------------------------------------------------

juce_add_console_app(TuneListTests
    PRODUCT_NAME "TuneListTests"
)

target_sources(TuneListTests PRIVATE
    Source/Tests/TestMain.cpp
    Source/Tests/TrackFilenameParserTests.cpp
    Source/Tests/TrackLibraryTaggingTests.cpp
    Source/Tests/TrackPreviewPlayerTests.cpp
    Source/Tests/TrackStorageTests.cpp
    Source/Tests/SpotifyClientTests.cpp
    Source/Tests/TuneListConfigTests.cpp
    Source/Tests/MetadataProxyClientTests.cpp
    Source/Tests/LookupMatchingTests.cpp
    Source/TuneListConfig.cpp
    Source/TuneListConfig.h
    Source/SpotifyClient.cpp
    Source/SpotifyClient.h
    Source/MetadataProxyClient.cpp
    Source/MetadataProxyClient.h
    Source/LookupMatching.cpp
    Source/LookupMatching.h
    Source/TrackFilenameParser.cpp
    Source/TrackFilenameParser.h
    Source/TrackFile.h
    Source/TrackLibrary.cpp
    Source/TrackLibrary.h
    Source/TrackPreviewPlayer.cpp
    Source/TrackPreviewPlayer.h
    Source/TrackStorage.cpp
    Source/TrackStorage.h
)

target_compile_definitions(TuneListTests PRIVATE
    JUCE_USE_CURL=0
    JUCE_WEB_BROWSER=0
    JUCE_STANDALONE_APPLICATION=1
    JUCE_UNIT_TESTS=1
    JUCE_USE_MP3AUDIOFORMAT=1
    DONT_SET_USING_JUCE_NAMESPACE=1
)

target_link_libraries(TuneListTests PRIVATE
    juce::juce_audio_basics
    juce::juce_audio_devices
    juce::juce_audio_formats
    juce::juce_core
    juce::juce_data_structures
    juce::juce_events
    juce::juce_graphics
    juce::juce_gui_basics
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags
)

enable_testing()
add_test(NAME TuneListTests COMMAND TuneListTests)
