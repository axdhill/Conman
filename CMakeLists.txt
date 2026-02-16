cmake_minimum_required(VERSION 3.22)
project(Conman VERSION 1.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.4
)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(Conman
    COMPANY_NAME "protist"
    PLUGIN_MANUFACTURER_CODE Cvpl
    PLUGIN_CODE Cvpg
    FORMATS AUv3 VST3 Standalone
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    PRODUCT_NAME "Conman"
)

target_sources(Conman
    PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp
        Source/OfflineConvolver.cpp
)

target_compile_definitions(Conman
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(Conman
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)
