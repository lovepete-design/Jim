cmake_minimum_required(VERSION 3.22)
project(TriadTune VERSION 0.1.0 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
option(TRIAD_ENGINE_ONLY "Build DSP tests without JUCE or GUI dependencies" OFF)
include(CTest)
if(BUILD_TESTING)
    add_executable(triad_engine_tests tests/EngineTests.cpp)
    target_include_directories(triad_engine_tests PRIVATE Source)
    add_test(NAME engine COMMAND triad_engine_tests)
endif()
if(TRIAD_ENGINE_ONLY)
    return()
endif()
set(JUCE_DIR "" CACHE PATH "Optional local JUCE checkout")
if(JUCE_DIR)
    add_subdirectory("${JUCE_DIR}" juce)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE URL https://github.com/juce-framework/JUCE/archive/refs/tags/8.0.6.tar.gz
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()
set(plugin_formats VST3 Standalone)
if(APPLE)
    list(APPEND plugin_formats AU)
endif()
juce_add_plugin(TriadTune
    COMPANY_NAME "Triad Audio"
    BUNDLE_ID "audio.triad.tune"
    PLUGIN_MANUFACTURER_CODE Tria
    PLUGIN_CODE TrTu
    FORMATS ${plugin_formats}
    PRODUCT_NAME "Triad Tune"
    DESCRIPTION "Monophonic vocal pitch correction"
    IS_SYNTH FALSE NEEDS_MIDI_INPUT FALSE NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE COPY_PLUGIN_AFTER_BUILD FALSE
    MICROPHONE_PERMISSION_ENABLED TRUE
    MICROPHONE_PERMISSION_TEXT "Triad Tune uses your microphone for live vocal pitch correction."
    VST3_CATEGORIES Fx "Pitch Shift")
target_sources(TriadTune PRIVATE Source/PluginProcessor.cpp Source/PluginEditor.cpp)
target_compile_definitions(TriadTune PUBLIC JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)
target_link_libraries(TriadTune PRIVATE juce::juce_audio_utils
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
if(BUILD_TESTING)
    add_executable(triad_plugin_tests tests/PluginTests.cpp)
    target_link_libraries(triad_plugin_tests PRIVATE TriadTune)
    target_include_directories(triad_plugin_tests PRIVATE Source $<TARGET_PROPERTY:TriadTune,INCLUDE_DIRECTORIES>)
    target_compile_definitions(triad_plugin_tests PRIVATE $<TARGET_PROPERTY:TriadTune,COMPILE_DEFINITIONS>)
    add_test(NAME plugin_state_and_transport COMMAND triad_plugin_tests)
endif()
