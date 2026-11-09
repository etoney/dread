cmake_minimum_required(VERSION 3.22)
project(DREAD VERSION 1.9.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "")
set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "")

include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.15
    GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(DREAD
    COMPANY_NAME "Castle Danger"
    PRODUCT_NAME "DREAD"
    BUNDLE_ID com.castledanger.dread
    PLUGIN_MANUFACTURER_CODE Cdgr
    PLUGIN_CODE Drd1
    FORMATS VST3 AU Standalone
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Instrument Synth
    AU_MAIN_TYPE kAudioUnitType_MusicDevice)

target_sources(DREAD PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_compile_definitions(DREAD PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(DREAD
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# Offline DSP test (no JUCE): renders test/demo.wav
add_executable(dread_render test/render_demo.cpp)
add_executable(dread_render_new test/render_new_sounds.cpp)
add_executable(dread_render_chains test/render_chains_stings.cpp)
add_executable(dread_render_symphony test/render_symphony.cpp)
add_executable(dread_render_v17 test/render_v17.cpp)
add_executable(dread_raise_hell_test test/raise_hell_test.cpp)
target_include_directories(dread_raise_hell_test PRIVATE Source)
target_include_directories(dread_render_v17 PRIVATE Source)
target_include_directories(dread_render_symphony PRIVATE Source)
target_include_directories(dread_render_chains PRIVATE Source)
target_include_directories(dread_render_new PRIVATE Source)
target_include_directories(dread_render PRIVATE Source)

# Headless GUI snapshot (CI renders the editor to gui.png for review)
juce_add_console_app(DreadSnapshot PRODUCT_NAME "DreadSnapshot")
target_sources(DreadSnapshot PRIVATE test/snapshot.cpp Source/PluginProcessor.cpp Source/PluginEditor.cpp)
target_compile_definitions(DreadSnapshot PRIVATE
    JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0
    "JucePlugin_VersionString=\"${PROJECT_VERSION}\"")
target_link_libraries(DreadSnapshot PRIVATE juce::juce_audio_utils juce::juce_dsp PUBLIC juce::juce_recommended_config_flags)
