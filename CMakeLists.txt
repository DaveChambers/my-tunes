cmake_minimum_required(VERSION 3.22)
project(MyTunes VERSION 0.1.0 LANGUAGES C CXX OBJC OBJCXX)

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

juce_add_gui_app(MyTunes
    PRODUCT_NAME    "My Tunes"
    BUNDLE_ID       "com.makeelectronicsound.mytunes"
    COMPANY_NAME    "Make Electronic Sound"
    COMPANY_WEBSITE "https://makeelectronicsound.com"
)

target_sources(MyTunes PRIVATE
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
    Source/ArtworkCache.cpp
    Source/ArtworkCache.h
    Source/TrackListComponent.cpp
    Source/TrackListComponent.h
    Source/TrackGridComponent.cpp
    Source/TrackGridComponent.h
    Source/EmojiTagBoardComponent.cpp
    Source/EmojiTagBoardComponent.h
    Source/TrackDetailComponent.cpp
    Source/TrackDetailComponent.h
)

target_compile_definitions(MyTunes PRIVATE
    JUCE_USE_CURL=0
    JUCE_WEB_BROWSER=0
    JUCE_STANDALONE_APPLICATION=1
    DONT_SET_USING_JUCE_NAMESPACE=1
    MYTUNES_VERSION_STRING="${PROJECT_VERSION}"
)

target_link_libraries(MyTunes PRIVATE
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

juce_add_console_app(MyTunesTests
    PRODUCT_NAME "MyTunesTests"
)

target_sources(MyTunesTests PRIVATE
    Source/Tests/TestMain.cpp
    Source/Tests/TrackFilenameParserTests.cpp
    Source/Tests/TrackLibraryTaggingTests.cpp
    Source/TrackFilenameParser.cpp
    Source/TrackFilenameParser.h
    Source/TrackFile.h
    Source/TrackLibrary.cpp
    Source/TrackLibrary.h
)

target_compile_definitions(MyTunesTests PRIVATE
    JUCE_USE_CURL=0
    JUCE_WEB_BROWSER=0
    JUCE_STANDALONE_APPLICATION=1
    JUCE_UNIT_TESTS=1
    DONT_SET_USING_JUCE_NAMESPACE=1
)

target_link_libraries(MyTunesTests PRIVATE
    juce::juce_core
    juce::juce_data_structures
    juce::juce_events
    juce::juce_graphics
    juce::juce_gui_basics
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags
)

enable_testing()
add_test(NAME MyTunesTests COMMAND MyTunesTests)
