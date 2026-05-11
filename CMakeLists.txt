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

# Bundle icon: JUCE bakes ICON_BIG into the app's Info.plist + Resources
# at configure time (generates Icon.icns inside the .app), so the source
# PNG must exist at configure time. The TuneList icon lives at
# Assets/TuneListIcon.png; if for some reason it's missing we fall back
# to building without a custom icon rather than failing configure.
#
# BUNDLE_ID is intentionally kept on the legacy "mytunes" reverse-DNS so
# that previously-saved user state under
#   ~/Library/Application Support/My Tunes/
# remains the canonical store across the rename to TuneList.
set(_app_icon_args "")
set(_app_icon_path "${CMAKE_CURRENT_SOURCE_DIR}/Assets/TuneListIcon.png")
if(EXISTS "${_app_icon_path}")
    list(APPEND _app_icon_args ICON_BIG "${_app_icon_path}")
endif()

juce_add_gui_app(TuneList
    ${_app_icon_args}
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
    Source/DiscogsClient.cpp
    Source/DiscogsClient.h
    Source/SpotifyClient.cpp
    Source/SpotifyClient.h
    Source/ArtworkCache.cpp
    Source/ArtworkCache.h
    Source/LookupController.cpp
    Source/LookupController.h
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
    Source/SpotifyClient.cpp
    Source/SpotifyClient.h
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
