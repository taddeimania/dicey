cmake_minimum_required(VERSION 3.22)

# Apple silicon, macOS 11+. Set before project().
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")
set(CMAKE_OSX_ARCHITECTURES "arm64" CACHE STRING "Build architectures")

project(Dicey VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- JUCE ---------------------------------------------------------------------
# By default JUCE is downloaded on first configure. To use a local checkout:
#   cmake -B build -DDICEY_JUCE_DIR=/path/to/JUCE
set(DICEY_JUCE_DIR "" CACHE PATH "Optional local JUCE checkout")

if(DICEY_JUCE_DIR)
    add_subdirectory(${DICEY_JUCE_DIR} JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG        8.0.9
        GIT_SHALLOW    TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

# --- Plugin -------------------------------------------------------------------
juce_add_plugin(Dicey
    COMPANY_NAME                "Taddei"
    BUNDLE_ID                   com.taddei.dicey
    PLUGIN_MANUFACTURER_CODE    Tadd
    PLUGIN_CODE                 Dicy
    PRODUCT_NAME                "Dicey"
    FORMATS                     VST3
    IS_SYNTH                    TRUE
    NEEDS_MIDI_INPUT            TRUE
    NEEDS_MIDI_OUTPUT           TRUE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    VST3_CATEGORIES             Instrument Generator
    COPY_PLUGIN_AFTER_BUILD     TRUE)   # installs to ~/Library/Audio/Plug-Ins/VST3

target_sources(Dicey PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/UI/Widgets.cpp)

target_include_directories(Dicey PRIVATE Source)

target_compile_definitions(Dicey PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(Dicey
    PRIVATE
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# --- Engine tests (no JUCE needed) --------------------------------------------
enable_testing()
add_executable(DiceyEngineTests Tests/EngineTests.cpp)
add_test(NAME engine COMMAND DiceyEngineTests)
