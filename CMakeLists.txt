cmake_minimum_required(VERSION 3.22)
project(TriadAudio VERSION 0.2.2 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
option(TRIAD_BUILD_PLUGINS "Build VST3 plugins and host smoke tests" ON)
option(TRIAD_SANITIZE "Enable undefined-behaviour/address sanitizers for DSP tests" OFF)
include(CTest)

add_library(triad_dsp STATIC
    plugins/hex/HexEngine.cpp
    plugins/stress/Dynamics.cpp
    plugins/lattice/Lattice.cpp
    third_party/msfa/dx7note.cc third_party/msfa/env.cc
    third_party/msfa/exp2.cc third_party/msfa/fm_core.cc
    third_party/msfa/fm_op_kernel.cc third_party/msfa/freqlut.cc
    third_party/msfa/lfo.cc third_party/msfa/pitchenv.cc
    third_party/msfa/sin.cc third_party/msfa/EngineMkI.cpp)
target_include_directories(triad_dsp PUBLIC plugins/hex plugins/stress plugins/lattice PRIVATE third_party/msfa)
if(MSVC)
    target_compile_options(triad_dsp PRIVATE /W3)
else()
    target_compile_options(triad_dsp PRIVATE -Wall -Wextra -Wno-unused-parameter)
    # The original fixed-point DX7 engine relies on two's-complement wrapping.
    set_source_files_properties(third_party/msfa/dx7note.cc third_party/msfa/env.cc
        third_party/msfa/exp2.cc third_party/msfa/fm_core.cc
        third_party/msfa/fm_op_kernel.cc third_party/msfa/freqlut.cc
        third_party/msfa/lfo.cc third_party/msfa/pitchenv.cc
        third_party/msfa/sin.cc third_party/msfa/EngineMkI.cpp
        PROPERTIES COMPILE_OPTIONS "-fwrapv")
endif()
add_executable(triad_tests tests/Main.cpp tests/HexTests.cpp tests/DynamicsTests.cpp tests/LatticeTests.cpp)
target_link_libraries(triad_tests PRIVATE triad_dsp)
target_include_directories(triad_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
if(TRIAD_SANITIZE AND NOT MSVC)
    target_compile_options(triad_dsp PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(triad_dsp INTERFACE -fsanitize=address,undefined)
    target_compile_options(triad_tests PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
endif()
add_test(NAME dsp COMMAND triad_tests)

if(TRIAD_BUILD_PLUGINS)
    if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/third_party/JUCE/CMakeLists.txt")
        include(FetchContent)
        FetchContent_Declare(JUCE
            SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/JUCE"
            GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
            GIT_TAG 51a8a6d7aeae7326956d747737ccf1575e61e209
            GIT_SHALLOW FALSE)
        FetchContent_MakeAvailable(JUCE)
    else()
        add_subdirectory(third_party/JUCE "${CMAKE_CURRENT_BINARY_DIR}/_deps/juce-build" EXCLUDE_FROM_ALL)
    endif()
    # Common audio utilities and the validation host's JUCE modules.
    add_library(triad_juce STATIC)
    target_link_libraries(triad_juce PRIVATE juce::juce_audio_utils)
    target_include_directories(triad_juce PUBLIC "$<TARGET_PROPERTY:juce::juce_audio_utils,INTERFACE_INCLUDE_DIRECTORIES>")
    target_compile_definitions(triad_juce PUBLIC
        JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_USE_FLAC=0 JUCE_USE_OGGVORBIS=0
        JUCE_VST3_CAN_REPLACE_VST2=0 JUCE_DISPLAY_SPLASH_SCREEN=0
        JUCE_REPORT_APP_USAGE=0 JUCE_PLUGINHOST_VST3=1 JUCE_GLOBAL_MODULE_SETTINGS_INCLUDED=1
        JUCE_MODAL_LOOPS_PERMITTED=1)
    # JUCE module targets expose implementation files as interface sources.
    # Move those implementations to the common library so they aren't compiled
    # again for every instrument. Keep their include/definition/link interfaces.
    foreach(module IN ITEMS juce_audio_utils juce_audio_processors juce_gui_extra
            juce_gui_basics juce_graphics juce_events juce_core
            juce_data_structures juce_audio_basics juce_audio_formats juce_audio_devices)
        get_target_property(module_sources ${module} INTERFACE_SOURCES)
        if(module_sources)
            target_sources(triad_juce PRIVATE ${module_sources})
            set_property(TARGET ${module} PROPERTY INTERFACE_SOURCES "")
        endif()
    endforeach()
    function(triad_plugin target title code kind synth category)
        string(REPLACE "|" ";" categories "${category}")
        juce_add_plugin(${target}
            COMPANY_NAME "Triad Audio" BUNDLE_ID "audio.triad.${code}"
            PLUGIN_MANUFACTURER_CODE Tria PLUGIN_CODE ${code}
            FORMATS VST3 PRODUCT_NAME "${title}"
            IS_SYNTH ${synth} NEEDS_MIDI_INPUT ${synth} NEEDS_MIDI_OUTPUT FALSE
            IS_MIDI_EFFECT FALSE EDITOR_WANTS_KEYBOARD_FOCUS FALSE
            COPY_PLUGIN_AFTER_BUILD FALSE VST3_CATEGORIES ${categories})
        target_sources(${target} PRIVATE shared/Plugin.cpp shared/Plugin.h shared/Editor.cpp shared/Editor.h)
        target_compile_definitions(${target} PUBLIC TRIAD_KIND=${kind})
        target_link_libraries(${target} PRIVATE triad_dsp triad_juce
            PUBLIC juce::juce_recommended_config_flags)
        if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
            target_link_options(${target}_VST3 PRIVATE "LINKER:--exclude-libs,ALL")
        elseif(APPLE)
            # Keep each plugin's JUCE singletons private to its Mach-O image.
            # Only the VST3 factory and bundle lifecycle are host entry points.
            set(exports_file "${CMAKE_CURRENT_SOURCE_DIR}/shared/vst3.exports")
            target_link_options(${target}_VST3 PRIVATE "LINKER:-exported_symbols_list,${exports_file}")
            set_property(TARGET ${target}_VST3 APPEND PROPERTY LINK_DEPENDS "${exports_file}")
        endif()
    endfunction()
    triad_plugin(HexMkI "Hex Library" HxL2 0 TRUE "Instrument|Synth")
    target_sources(HexMkI PRIVATE shared/HexLibrary.cpp shared/HexLibrary.h shared/HexBrowser.cpp shared/HexBrowser.h)
    triad_plugin(Stress "Stress" Strs 1 FALSE "Fx|Dynamics")
    triad_plugin(Lattice "Lattice" Latt 2 TRUE "Instrument|Synth")
    add_custom_target(plugins DEPENDS HexMkI_VST3 Stress_VST3 Lattice_VST3)

    juce_add_console_app(triad_host PRODUCT_NAME "Triad Host Validation")
    target_sources(triad_host PRIVATE tests/Host.cpp)
    target_compile_definitions(triad_host PRIVATE JUCE_PLUGINHOST_VST3=1)
    target_link_libraries(triad_host PRIVATE triad_juce)
    add_dependencies(triad_host plugins)
    add_test(NAME vst3_host COMMAND triad_host
        "${CMAKE_CURRENT_BINARY_DIR}/HexMkI_artefacts/$<CONFIG>/VST3/Hex Library.vst3"
        "${CMAKE_CURRENT_BINARY_DIR}/Stress_artefacts/$<CONFIG>/VST3/Stress.vst3"
        "${CMAKE_CURRENT_BINARY_DIR}/Lattice_artefacts/$<CONFIG>/VST3/Lattice.vst3")
    set_tests_properties(vst3_host PROPERTIES TIMEOUT 120)

    juce_add_console_app(triad_library_tests PRODUCT_NAME "Hex Library Validation")
    target_sources(triad_library_tests PRIVATE tests/HexLibraryTests.cpp shared/HexLibrary.cpp)
    target_link_libraries(triad_library_tests PRIVATE triad_dsp triad_juce)
    add_test(NAME hex_library COMMAND triad_library_tests)
    set_tests_properties(hex_library PROPERTIES TIMEOUT 120)
    juce_add_console_app(triad_hex_state_tests PRODUCT_NAME "Hex State Validation")
    target_sources(triad_hex_state_tests PRIVATE tests/HexStateTests.cpp)
    target_link_libraries(triad_hex_state_tests PRIVATE HexMkI triad_juce)
    add_test(NAME hex_state COMMAND triad_hex_state_tests)
    set_tests_properties(hex_state PROPERTIES TIMEOUT 60)
endif()
