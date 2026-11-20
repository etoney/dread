cmake_minimum_required(VERSION 3.22)
# Must come BEFORE project(), or CMake ignores them: universal (Apple Silicon + Intel), macOS 11+
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "")
set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "")
project(TORMENT VERSION 1.11.2)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.15
    GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(Torment
    COMPANY_NAME "Castle Danger"
    PRODUCT_NAME "Torment"
    BUNDLE_ID com.castledanger.torment
    PLUGIN_MANUFACTURER_CODE Cdgr
    PLUGIN_CODE Trm1
    FORMATS VST3 AU Standalone
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Instrument Synth
    AU_MAIN_TYPE kAudioUnitType_MusicDevice)

target_sources(Torment PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_compile_definitions(Torment PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(Torment
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# Offline DSP test (no JUCE): renders test/demo.wav
add_executable(torment_render test/render_demo.cpp)
add_executable(torment_render_new test/render_new_sounds.cpp)
add_executable(torment_render_chains test/render_chains_stings.cpp)
add_executable(torment_render_symphony test/render_symphony.cpp)
add_executable(torment_render_v17 test/render_v17.cpp)
add_executable(torment_raise_hell_test test/raise_hell_test.cpp)
add_executable(torment_render_bells_strings test/render_bells_strings.cpp)
target_include_directories(torment_render_bells_strings PRIVATE Source)
target_include_directories(torment_raise_hell_test PRIVATE Source)
target_include_directories(torment_render_v17 PRIVATE Source)
target_include_directories(torment_render_symphony PRIVATE Source)
target_include_directories(torment_render_chains PRIVATE Source)
target_include_directories(torment_render_new PRIVATE Source)
target_include_directories(torment_render PRIVATE Source)

# Headless GUI snapshot (CI renders the editor to gui.png for review)
juce_add_console_app(TormentSnapshot PRODUCT_NAME "TormentSnapshot")
target_sources(TormentSnapshot PRIVATE test/snapshot.cpp Source/PluginProcessor.cpp Source/PluginEditor.cpp)
target_compile_definitions(TormentSnapshot PRIVATE
    JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0
    "JucePlugin_VersionString=\"${PROJECT_VERSION}\"")
target_link_libraries(TormentSnapshot PRIVATE juce::juce_audio_utils juce::juce_dsp PUBLIC juce::juce_recommended_config_flags)
