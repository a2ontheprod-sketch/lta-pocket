cmake_minimum_required(VERSION 3.22)
project(LTA_Pocket VERSION 1.0.0 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
# Pin JUCE for reproducible Windows builds. GitHub Actions downloads it automatically.
FetchContent_Declare(JUCE
    URL https://github.com/juce-framework/JUCE/archive/refs/tags/8.0.6.zip
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
FetchContent_MakeAvailable(JUCE)

# Embed the approved flat mockup inside the VST3 - no extra artwork installed by user.
juce_add_binary_data(PocketArtwork SOURCES Assets/pocket_background.png)

juce_add_plugin(LTA_Pocket
    COMPANY_NAME "LTA Labs"
    BUNDLE_ID "com.ltalabs.pocket"
    PLUGIN_MANUFACTURER_CODE Ltaa
    PLUGIN_CODE Lpoc
    FORMATS VST3
    PRODUCT_NAME "LTA Pocket"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE)

target_sources(LTA_Pocket PRIVATE
    Source/PluginProcessor.h
    Source/PluginProcessor.cpp
    Source/PluginEditor.h
    Source/PluginEditor.cpp
    Source/PocketDSP.h)

target_compile_definitions(LTA_Pocket PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(LTA_Pocket PRIVATE
    PocketArtwork
    juce::juce_audio_utils
    juce::juce_gui_extra
    juce::juce_dsp
    PUBLIC juce::juce_recommended_config_flags)

juce_generate_juce_header(LTA_Pocket)
