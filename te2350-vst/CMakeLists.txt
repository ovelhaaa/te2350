cmake_minimum_required(VERSION 3.22)

project(TE2350AntigravityPlugin VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
include(CTest)

option(TE2350_FETCH_JUCE "Fetch JUCE with CMake FetchContent when TE2350_JUCE_PATH is not set" ON)
set(TE2350_JUCE_PATH "" CACHE PATH "Path to a local JUCE checkout")
set(TE2350_JUCE_TAG "8.0.8" CACHE STRING "JUCE tag used when TE2350_FETCH_JUCE is enabled")
set(TE2350_MAIN_DELAY_SIZE "32768" CACHE STRING "Main TE-2350 delay size for Hardware Mode parity")
set(TE2350_PLUGIN_COPY_DIR "${CMAKE_CURRENT_BINARY_DIR}/InstalledPlugins" CACHE PATH "Writable plugin copy directory")

get_filename_component(TE2350_ROOT "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)

if(TE2350_JUCE_PATH)
    add_subdirectory("${TE2350_JUCE_PATH}" JUCE)
elseif(TE2350_FETCH_JUCE)
    include(FetchContent)
    FetchContent_Declare(
        JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG ${TE2350_JUCE_TAG}
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(JUCE)
else()
    message(FATAL_ERROR "Set TE2350_JUCE_PATH or enable TE2350_FETCH_JUCE")
endif()

add_library(te2350_core STATIC
    "${TE2350_ROOT}/src/te2350.c"
    "${TE2350_ROOT}/src/dsp_delay.c"
    "${TE2350_ROOT}/src/dsp_filters.c"
    "${TE2350_ROOT}/src/dsp_fdn.c"
    "${TE2350_ROOT}/src/dsp_modulation.c"
    "${TE2350_ROOT}/src/dsp_pitch.c"
    "${TE2350_ROOT}/src/dsp_melody.c"
)

target_include_directories(te2350_core PUBLIC "${TE2350_ROOT}/include")
target_compile_definitions(te2350_core PUBLIC TE_MAIN_DELAY_SIZE=${TE2350_MAIN_DELAY_SIZE})

set(TE2350_FORMATS VST3)
if(APPLE)
    list(APPEND TE2350_FORMATS AU)
endif()

juce_add_binary_data(TE2350Assets
    SOURCES
        Source/Assets/te2350_logo_custom.svg
)

juce_add_plugin(TE2350Antigravity
    COMPANY_NAME "TE-2350"
    BUNDLE_ID "com.te2350.antigravity"
    PLUGIN_MANUFACTURER_CODE TeAg
    PLUGIN_CODE T235
    FORMATS ${TE2350_FORMATS}
    PRODUCT_NAME "TE-2350 Antigravity"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    VST3_COPY_DIR "${TE2350_PLUGIN_COPY_DIR}/VST3"
    AU_COPY_DIR "${TE2350_PLUGIN_COPY_DIR}/AU"
    COPY_PLUGIN_AFTER_BUILD FALSE
)

target_sources(TE2350Antigravity PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/Core/TE2350CoreWrapper.cpp
    Source/Params/ParameterLayout.cpp
    Source/Params/MacroEngine.cpp
    Source/Presets/FactoryPresets.cpp
    Source/DSP/OversamplingChain.cpp
)

target_include_directories(TE2350Antigravity PRIVATE
    Source
    "${TE2350_ROOT}/include"
)

target_compile_definitions(TE2350Antigravity PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    TE2350_PLUGIN_BUILD=1
)

if(TARGET TE2350Antigravity_VST3)
    target_compile_definitions(TE2350Antigravity_VST3 PRIVATE JUCE_VST3_CAN_REPLACE_VST2=0)
endif()

target_link_libraries(TE2350Antigravity PRIVATE
    te2350_core
    TE2350Assets
    juce::juce_audio_utils
    juce::juce_dsp
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)

option(TE2350_BUILD_GOLDEN_TESTS "Build the golden-reference scaffold executable" OFF)
if(TE2350_BUILD_GOLDEN_TESTS)
    add_executable(TE2350OfflineReference "${TE2350_ROOT}/src/web/offline_host.c")
    target_include_directories(TE2350OfflineReference PRIVATE "${TE2350_ROOT}/include")
    target_link_libraries(TE2350OfflineReference PRIVATE te2350_core)

    add_executable(TE2350GoldenReference Tests/GoldenReferenceTest.cpp)
    target_include_directories(TE2350GoldenReference PRIVATE Source "${TE2350_ROOT}/include")
    target_link_libraries(TE2350GoldenReference PRIVATE te2350_core juce::juce_audio_basics)

    set(TE2350_GOLDEN_OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/GoldenReferenceOutput")
    file(MAKE_DIRECTORY "${TE2350_GOLDEN_OUTPUT_DIR}")
    add_test(NAME TE2350OfflineReferenceRender COMMAND $<TARGET_FILE:TE2350OfflineReference>)
    add_test(NAME TE2350GoldenReferenceRender COMMAND $<TARGET_FILE:TE2350GoldenReference>)
    add_test(NAME TE2350GoldenReferenceCompare
        COMMAND "${CMAKE_COMMAND}" -E compare_files output.raw plugin_reference.raw)

    set_tests_properties(TE2350OfflineReferenceRender TE2350GoldenReferenceRender TE2350GoldenReferenceCompare PROPERTIES
        WORKING_DIRECTORY "${TE2350_GOLDEN_OUTPUT_DIR}")
    set_tests_properties(TE2350GoldenReferenceCompare PROPERTIES
        DEPENDS "TE2350OfflineReferenceRender;TE2350GoldenReferenceRender")

    add_executable(TE2350PluginSmokeTest Tests/PluginSmokeTest.cpp)
    target_include_directories(TE2350PluginSmokeTest PRIVATE Source "${TE2350_ROOT}/include")
    target_compile_definitions(TE2350PluginSmokeTest PRIVATE
        JucePlugin_Name="TE-2350 Antigravity"
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0)
    target_link_libraries(TE2350PluginSmokeTest PRIVATE
        TE2350Antigravity
        juce::juce_audio_utils
        juce::juce_dsp)
    add_test(NAME TE2350PluginSmokeTest COMMAND $<TARGET_FILE:TE2350PluginSmokeTest>)
endif()
