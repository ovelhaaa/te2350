cmake_minimum_required(VERSION 3.22)

project(TE2350AntigravityPlugin VERSION 0.2.0 LANGUAGES C CXX)

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
set(TE2350_JUCE_TAG "8.0.8" CACHE STRING "JUCE tag used when TE2350_FETCH_JUCE is enabled")
set(TE2350_MAIN_DELAY_SIZE "524288" CACHE STRING "Desktop main delay size (power of two; 524288 covers 2 s through 192 kHz)")
math(EXPR TE2350_MAIN_DELAY_SIZE_MASK "${TE2350_MAIN_DELAY_SIZE} & (${TE2350_MAIN_DELAY_SIZE} - 1)")
if(TE2350_MAIN_DELAY_SIZE LESS 4 OR NOT TE2350_MAIN_DELAY_SIZE_MASK EQUAL 0)
    message(FATAL_ERROR "TE2350_MAIN_DELAY_SIZE must be a power of two")
endif()
set(TE2350_PLUGIN_COPY_DIR "${CMAKE_CURRENT_BINARY_DIR}/InstalledPlugins" CACHE PATH "Writable plugin copy directory")
set(TE2350_RELEASE_CHANNEL "rc1" CACHE STRING "Pre-release channel appended to distribution packages")
set(TE2350_PACKAGE_OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/Packages" CACHE PATH "Distribution package output directory")

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

set(TE2350_FORMATS VST3 Standalone)
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

string(TOLOWER "${CMAKE_SYSTEM_NAME}" TE2350_PACKAGE_SYSTEM)
string(TOLOWER "${CMAKE_SYSTEM_PROCESSOR}" TE2350_PACKAGE_ARCH)
if(NOT TE2350_PACKAGE_ARCH)
    set(TE2350_PACKAGE_ARCH "unknown-arch")
endif()
set(TE2350_RELEASE_VERSION "${PROJECT_VERSION}-${TE2350_RELEASE_CHANNEL}")
set(TE2350_PACKAGE_NAME
    "TE2350-Antigravity-${TE2350_RELEASE_VERSION}-${TE2350_PACKAGE_SYSTEM}-${TE2350_PACKAGE_ARCH}")

add_custom_target(TE2350PackageBeta
    COMMAND "${CMAKE_COMMAND}"
        "-DTE2350_VST3_BINARY=$<TARGET_FILE:TE2350Antigravity_VST3>"
        "-DTE2350_PACKAGE_OUTPUT_DIR=${TE2350_PACKAGE_OUTPUT_DIR}"
        "-DTE2350_PACKAGE_NAME=${TE2350_PACKAGE_NAME}"
        "-DTE2350_RELEASE_VERSION=${TE2350_RELEASE_VERSION}"
        "-DTE2350_BUILD_CONFIGURATION=$<CONFIG>"
        "-DTE2350_SYSTEM_NAME=${CMAKE_SYSTEM_NAME}"
        "-DTE2350_SYSTEM_PROCESSOR=${CMAKE_SYSTEM_PROCESSOR}"
        "-DTE2350_INSTALL_GUIDE=${CMAKE_CURRENT_SOURCE_DIR}/Packaging/INSTALL.md"
        "-DTE2350_CHANGELOG=${CMAKE_CURRENT_SOURCE_DIR}/CHANGELOG.md"
        "-DTE2350_PLUGIN_README=${CMAKE_CURRENT_SOURCE_DIR}/README.md"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/PackageBeta.cmake"
    DEPENDS TE2350Antigravity_VST3
    COMMENT "Packaging TE-2350 Antigravity ${TE2350_RELEASE_VERSION} VST3"
    VERBATIM
)

option(TE2350_BUILD_GOLDEN_TESTS "Build the golden-reference scaffold executable" OFF)
if(TE2350_BUILD_GOLDEN_TESTS)
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
            "${CMAKE_CURRENT_BINARY_DIR}/TE2350Antigravity_artefacts/VST3/TE-2350 Antigravity.vst3")

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

    add_executable(TE2350PresetWorkflowTest Tests/PresetWorkflowTest.cpp)
    target_include_directories(TE2350PresetWorkflowTest PRIVATE Source "${TE2350_ROOT}/include")
    target_compile_definitions(TE2350PresetWorkflowTest PRIVATE
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
endif()
