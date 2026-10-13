cmake_minimum_required(VERSION 3.22)

# Universal binary so it runs on both Apple Silicon and Intel Macs
set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "")
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "")

project(BowString VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(BOWSTRING_COPY_PLUGIN "Copy plugins into the system plug-in folders after building" ON)

include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.4
    GIT_SHALLOW    TRUE)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(BowString
    COMPANY_NAME              "Ali Hassan"
    PLUGIN_MANUFACTURER_CODE  Alih
    PLUGIN_CODE               Bows
    FORMATS                   AU VST3 Standalone
    PRODUCT_NAME              "BowString"
    IS_SYNTH                  FALSE
    NEEDS_MIDI_INPUT          FALSE
    NEEDS_MIDI_OUTPUT         FALSE
    IS_MIDI_EFFECT            FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    MICROPHONE_PERMISSION_ENABLED TRUE   # standalone app needs this to hear your guitar on macOS
    MICROPHONE_PERMISSION_TEXT "BowString needs audio input access to hear your guitar."
    COPY_PLUGIN_AFTER_BUILD   ${BOWSTRING_COPY_PLUGIN})

target_sources(BowString PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_compile_definitions(BowString PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(BowString
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
