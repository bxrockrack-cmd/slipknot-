cmake_minimum_required(VERSION 3.22)
project(SlipknotDrummer VERSION 0.1.0 LANGUAGES CXX)

include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.12
    GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(SlipknotDrummer
    COMPANY_NAME "BX Rock Rack"
    BUNDLE_ID "com.bxrockrack.slipknotdrummer"
    PLUGIN_MANUFACTURER_CODE Bxrk
    PLUGIN_CODE Slpk
    PRODUCT_NAME "Slipknot Drummer"
    DESCRIPTION "Metal drum sampler instrument"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    FORMATS VST3
    VST3_CATEGORIES Instrument Drum Sampler)

target_sources(SlipknotDrummer PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginProcessor.h
    Source/PluginEditor.cpp
    Source/PluginEditor.h)

target_compile_features(SlipknotDrummer PRIVATE cxx_std_17)
target_compile_definitions(SlipknotDrummer PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(SlipknotDrummer PRIVATE
    juce::juce_audio_utils
    juce::juce_audio_formats
    juce::juce_dsp
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags)

# Создаёт JuceHeader.h. Вызывать только после target_link_libraries.
juce_generate_juce_header(SlipknotDrummer)
