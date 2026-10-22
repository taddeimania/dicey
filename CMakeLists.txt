cmake_minimum_required(VERSION 3.22)

# macOS 11+. Builds for the machine you're on by default; for a universal
# binary pass -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64". Set before project().
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")

project(Dicey VERSION 1.0.1 LANGUAGES C CXX)

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
# Copy the built plugin into the system VST3 folder after each build:
#   macOS   ~/Library/Audio/Plug-Ins/VST3
#   Linux   ~/.vst3
#   Windows C:\Program Files\Common Files\VST3  (needs an administrator shell,
#           so it is off by default there)
if(WIN32)
    set(_dicey_copy_default OFF)
else()
    set(_dicey_copy_default ON)
endif()
option(DICEY_COPY_PLUGIN "Install the plugin into the system VST3 folder after building" ${_dicey_copy_default})

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
    COPY_PLUGIN_AFTER_BUILD     ${DICEY_COPY_PLUGIN})

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
