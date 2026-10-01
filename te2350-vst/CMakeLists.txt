cmake_minimum_required(VERSION 3.22)

project(TE2350AntigravityPlugin VERSION 0.3.0 LANGUAGES C CXX)

if(NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build configuration" FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS Debug Release RelWithDebInfo MinSizeRel)
endif()

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
include(CTest)

option(TE2350_FETCH_JUCE "Fetch JUCE with CMake FetchContent when TE2350_JUCE_PATH is not set" ON)
set(TE2350_JUCE_PATH "" CACHE PATH "Path to a local JUCE checkout")
# Fixed upstream tag, verified against the immutable commit after fetching.
set(TE2350_JUCE_TAG "7.0.12")
set(TE2350_JUCE_PIN "4f43011b96eb0636104cb3e433894cda98243626")
set(TE2350_MAIN_DELAY_SIZE "524288" CACHE STRING "Desktop main delay size (power of two; 524288 covers 2 s through 192 kHz)")
math(EXPR TE2350_MAIN_DELAY_SIZE_MASK "${TE2350_MAIN_DELAY_SIZE} & (${TE2350_MAIN_DELAY_SIZE} - 1)")
if(TE2350_MAIN_DELAY_SIZE LESS 4 OR NOT TE2350_MAIN_DELAY_SIZE_MASK EQUAL 0)
    message(FATAL_ERROR "TE2350_MAIN_DELAY_SIZE must be a power of two")
endif()
set(TE2350_PLUGIN_COPY_DIR "${CMAKE_CURRENT_BINARY_DIR}/InstalledPlugins" CACHE PATH "Writable plugin copy directory")
option(TE2350_COPY_PLUGIN_AFTER_BUILD "Copy to the explicit developer plugin directory" OFF)
set(TE2350_PACKAGE_OUTPUT_DIR "${CMAKE_CURRENT_LIST_DIR}/../dist" CACHE PATH "Distribution package output directory")

# Ship compiler runtimes statically; no developer-toolchain DLLs are required.
if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
elseif(MINGW)
    add_link_options(-static)
endif()

get_filename_component(TE2350_ROOT "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)

if(TE2350_JUCE_PATH)
    if(NOT EXISTS "${TE2350_JUCE_PATH}/modules/juce_core/juce_core.h"
       OR NOT EXISTS "${TE2350_JUCE_PATH}/extras/Build/CMake/JUCEUtils.cmake")
        message(FATAL_ERROR "TE2350_JUCE_PATH must point directly to a real JUCE checkout, not another project's wrapper")
    endif()
    file(READ "${TE2350_JUCE_PATH}/CMakeLists.txt" juce_cmake)
    if(NOT juce_cmake MATCHES "project\\(JUCE[ \r\n]")
        message(FATAL_ERROR "TE2350_JUCE_PATH is not the JUCE project root")
    endif()
    set(juce_SOURCE_DIR "${TE2350_JUCE_PATH}")
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

find_package(Git REQUIRED)
execute_process(COMMAND "${GIT_EXECUTABLE}" -C "${juce_SOURCE_DIR}" rev-parse HEAD
    OUTPUT_VARIABLE TE2350_JUCE_COMMIT OUTPUT_STRIP_TRAILING_WHITESPACE
    RESULT_VARIABLE juce_git_result ERROR_QUIET)
if(NOT TE2350_JUCE_PATH AND (NOT juce_git_result EQUAL 0 OR NOT TE2350_JUCE_COMMIT STREQUAL TE2350_JUCE_PIN))
    message(FATAL_ERROR "Fetched JUCE does not match the pinned 7.0.12 commit")
endif()
if(NOT juce_git_result EQUAL 0)
    set(TE2350_JUCE_COMMIT "unversioned-local-checkout")
endif()
file(READ "Source/Presets/UserPresetManager.h" preset_header)
string(REGEX MATCH "presetFormatVersion = ([0-9]+)" unused "${preset_header}")
set(TE2350_PRESET_FORMAT_VERSION "${CMAKE_MATCH_1}")
file(READ "Source/PluginProcessor.cpp" processor_source)
string(REGEX MATCH "currentStateVersion = ([0-9]+)" unused "${processor_source}")
set(TE2350_STATE_VERSION "${CMAKE_MATCH_1}")
configure_file(cmake/ReleaseMetadata.h.in generated/ReleaseMetadata.h @ONLY)
include_directories("${CMAKE_CURRENT_BINARY_DIR}/generated")

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

set(TE2350_FORMATS VST3 Standalone)
if(APPLE)
    list(APPEND TE2350_FORMATS AU)
endif()

juce_add_binary_data(TE2350Assets
    SOURCES
        Source/Assets/te2350_logo_custom.svg
)

juce_add_plugin(TE2350Antigravity
    VERSION "${PROJECT_VERSION}"
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
    COPY_PLUGIN_AFTER_BUILD ${TE2350_COPY_PLUGIN_AFTER_BUILD}
)

target_sources(TE2350Antigravity PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/Core/TE2350CoreWrapper.cpp
    Source/Params/ParameterLayout.cpp
    Source/Params/MacroEngine.cpp
    Source/Presets/FactoryPresets.cpp
    Source/Presets/UserPresetManager.cpp
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

option(TE2350_BUILD_GOLDEN_TESTS "Build the complete release qualification suite" ON)
if(TE2350_BUILD_GOLDEN_TESTS AND NOT BUILD_TESTING)
    message(FATAL_ERROR "Qualification requires BUILD_TESTING=ON")
endif()
if(TE2350_BUILD_GOLDEN_TESTS)
    add_executable(TE2350MacroCalibrationTest Tests/MacroCalibrationTest.cpp)
    target_include_directories(TE2350MacroCalibrationTest PRIVATE Source "${TE2350_ROOT}/include")
    target_compile_definitions(TE2350MacroCalibrationTest PRIVATE
        JucePlugin_Name="TE-2350 Antigravity" JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
    target_link_libraries(TE2350MacroCalibrationTest PRIVATE
        TE2350Antigravity juce::juce_audio_utils juce::juce_dsp)
    add_test(NAME TE2350MacroCalibrationTest COMMAND TE2350MacroCalibrationTest
        "${CMAKE_CURRENT_BINARY_DIR}/MacroCalibrationOutput")

    add_executable(TE2350OfflineReference "${TE2350_ROOT}/src/web/offline_host.c")
    target_include_directories(TE2350OfflineReference PRIVATE "${TE2350_ROOT}/include")
    target_link_libraries(TE2350OfflineReference PRIVATE te2350_core)

    add_executable(TE2350GoldenReference Tests/GoldenReferenceTest.cpp)
    target_include_directories(TE2350GoldenReference PRIVATE Source "${TE2350_ROOT}/include")
    target_link_libraries(TE2350GoldenReference PRIVATE te2350_core juce::juce_audio_basics)

    add_executable(TE2350CoreControlTest Tests/CoreControlTest.cpp)
    target_include_directories(TE2350CoreControlTest PRIVATE Source "${TE2350_ROOT}/include")
    target_link_libraries(TE2350CoreControlTest PRIVATE te2350_core)

    set(TE2350_GOLDEN_OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/GoldenReferenceOutput")
    file(MAKE_DIRECTORY "${TE2350_GOLDEN_OUTPUT_DIR}")
    add_test(NAME TE2350OfflineReferenceRender COMMAND $<TARGET_FILE:TE2350OfflineReference>)
    add_test(NAME TE2350GoldenReferenceRender COMMAND $<TARGET_FILE:TE2350GoldenReference>)
    add_test(NAME TE2350GoldenReferenceCompare
        COMMAND "${CMAKE_COMMAND}" -E compare_files output.raw plugin_reference.raw)
    add_test(NAME TE2350CoreControlTest COMMAND $<TARGET_FILE:TE2350CoreControlTest>)

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

    add_executable(TE2350CalibrationTest Tests/CalibrationTest.cpp)
    target_include_directories(TE2350CalibrationTest PRIVATE Source "${TE2350_ROOT}/include")
    target_compile_definitions(TE2350CalibrationTest PRIVATE
        JucePlugin_Name="TE-2350 Antigravity"
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0)
    target_link_libraries(TE2350CalibrationTest PRIVATE
        TE2350Antigravity
        juce::juce_audio_utils
        juce::juce_dsp)
    add_test(NAME TE2350CalibrationTest COMMAND $<TARGET_FILE:TE2350CalibrationTest>)

    add_executable(TE2350CalibrationRender Tests/CalibrationRender.cpp)
    target_include_directories(TE2350CalibrationRender PRIVATE Source "${TE2350_ROOT}/include")
    target_compile_definitions(TE2350CalibrationRender PRIVATE
        JucePlugin_Name="TE-2350 Antigravity"
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0)
    target_link_libraries(TE2350CalibrationRender PRIVATE
        TE2350Antigravity
        juce::juce_audio_utils
        juce::juce_dsp)

    add_executable(TE2350HostCompatibilityTest Tests/HostCompatibilityTest.cpp)
    target_include_directories(TE2350HostCompatibilityTest PRIVATE Source "${TE2350_ROOT}/include")
    target_compile_definitions(TE2350HostCompatibilityTest PRIVATE
        JucePlugin_Name="TE-2350 Antigravity"
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0)
    target_link_libraries(TE2350HostCompatibilityTest PRIVATE
        TE2350Antigravity
        juce::juce_audio_utils
        juce::juce_dsp)
    add_test(NAME TE2350HostCompatibilityTest COMMAND $<TARGET_FILE:TE2350HostCompatibilityTest>)

    add_executable(TE2350VST3LoadTest Tests/VST3LoadTest.cpp)
    target_include_directories(TE2350VST3LoadTest PRIVATE Source)
    target_compile_definitions(TE2350VST3LoadTest PRIVATE
        TE2350_EXPECTED_PLUGIN_VERSION="${PROJECT_VERSION}"
        JUCE_PLUGINHOST_VST3=1
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0)
    target_link_libraries(TE2350VST3LoadTest PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp)
    add_dependencies(TE2350VST3LoadTest TE2350Antigravity_VST3)
    add_test(NAME TE2350VST3LoadTest
        COMMAND $<TARGET_FILE:TE2350VST3LoadTest>
            "$<GENEX_EVAL:$<TARGET_PROPERTY:TE2350Antigravity_VST3,JUCE_PLUGIN_ARTEFACT_FILE>>")

    add_executable(TE2350UIRender Tests/UIRender.cpp)
    target_include_directories(TE2350UIRender PRIVATE Source "${TE2350_ROOT}/include")
    target_compile_definitions(TE2350UIRender PRIVATE
        JucePlugin_Name="TE-2350 Antigravity"
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0)
    target_link_libraries(TE2350UIRender PRIVATE
        TE2350Antigravity
        juce::juce_audio_utils
        juce::juce_dsp)
    add_test(NAME TE2350UIRenderTest
        COMMAND $<TARGET_FILE:TE2350UIRender>
            "${CMAKE_CURRENT_BINARY_DIR}/UIRenders")

    add_executable(TE2350PresetVoicingTest Tests/PresetVoicingTest.cpp)
    target_include_directories(TE2350PresetVoicingTest PRIVATE Source "${TE2350_ROOT}/include")
    target_compile_definitions(TE2350PresetVoicingTest PRIVATE
        JucePlugin_Name="TE-2350 Antigravity" JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
    target_link_libraries(TE2350PresetVoicingTest PRIVATE
        TE2350Antigravity juce::juce_audio_utils juce::juce_dsp)
    add_test(NAME TE2350PresetVoicingTest COMMAND TE2350PresetVoicingTest
        "${CMAKE_CURRENT_BINARY_DIR}/PresetVoicingOutput")

    add_executable(TE2350PresetWorkflowTest Tests/PresetWorkflowTest.cpp)
    target_include_directories(TE2350PresetWorkflowTest PRIVATE Source "${TE2350_ROOT}/include")
    target_compile_definitions(TE2350PresetWorkflowTest PRIVATE
        TE2350_M12_PRESET_FIXTURE="${CMAKE_CURRENT_SOURCE_DIR}/Tests/Fixtures/M12-v1.te2350preset"
        JucePlugin_Name="TE-2350 Antigravity"
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0)
    target_link_libraries(TE2350PresetWorkflowTest PRIVATE
        TE2350Antigravity
        juce::juce_audio_utils
        juce::juce_dsp)
    add_test(NAME TE2350PresetWorkflowTest COMMAND $<TARGET_FILE:TE2350PresetWorkflowTest>)

    add_executable(TE2350ReleaseReadinessTest Tests/ReleaseReadinessTest.cpp)
    target_include_directories(TE2350ReleaseReadinessTest PRIVATE Source "${TE2350_ROOT}/include")
    target_compile_definitions(TE2350ReleaseReadinessTest PRIVATE
        JucePlugin_Name="TE-2350 Antigravity"
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0)
    target_link_libraries(TE2350ReleaseReadinessTest PRIVATE
        TE2350Antigravity
        juce::juce_audio_utils
        juce::juce_dsp)
    add_test(NAME TE2350ReleaseReadinessTest COMMAND $<TARGET_FILE:TE2350ReleaseReadinessTest>)

    add_executable(TE2350MusicalBehaviourTest Tests/MusicalBehaviourTest.cpp)
    target_include_directories(TE2350MusicalBehaviourTest PRIVATE Source "${TE2350_ROOT}/include")
    target_compile_definitions(TE2350MusicalBehaviourTest PRIVATE
        JucePlugin_Name="TE-2350 Antigravity"
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0)
    target_link_libraries(TE2350MusicalBehaviourTest PRIVATE
        TE2350Antigravity
        juce::juce_audio_utils
        juce::juce_dsp)
    add_test(NAME TE2350MusicalBehaviourTest COMMAND $<TARGET_FILE:TE2350MusicalBehaviourTest>)
endif()

if(TE2350_BUILD_GOLDEN_TESTS)
    add_executable(TE2350FreezeConsistencyTest Tests/FreezeConsistencyTest.cpp)
    target_link_libraries(TE2350FreezeConsistencyTest PRIVATE te2350_core)
    file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/FreezeConsistencyOutput")
    add_test(NAME TE2350FreezeConsistencyTest COMMAND $<TARGET_FILE:TE2350FreezeConsistencyTest>)
    set_tests_properties(TE2350FreezeConsistencyTest PROPERTIES
        WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/FreezeConsistencyOutput")
endif()

include(cmake/Release.cmake)
