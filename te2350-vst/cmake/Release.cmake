# The Windows distribution is qualified; other formats remain developer builds.
foreach(flags IN ITEMS CMAKE_C_FLAGS CMAKE_CXX_FLAGS CMAKE_C_FLAGS_RELEASE
        CMAKE_CXX_FLAGS_RELEASE CMAKE_EXE_LINKER_FLAGS CMAKE_SHARED_LINKER_FLAGS)
    if("${${flags}}" MATCHES "sanitize|/RTC")
        message(FATAL_ERROR "Release qualification does not permit sanitizer/debug runtime flags: ${flags}")
    endif()
endforeach()
set(TE2350_QUALIFICATION_TARGETS
    TE2350MacroCalibrationTest TE2350OfflineReference TE2350GoldenReference
    TE2350CoreControlTest TE2350PluginSmokeTest TE2350CalibrationTest
    TE2350HostCompatibilityTest TE2350VST3LoadTest TE2350UIRender
    TE2350PresetVoicingTest TE2350PresetWorkflowTest TE2350ReleaseReadinessTest
    TE2350MusicalBehaviourTest TE2350FreezeConsistencyTest)
if(TE2350_BUILD_GOLDEN_TESTS)
    add_custom_target(release_qualification
        COMMAND "${CMAKE_COMMAND}" "-DBUILD_DIR=${CMAKE_BINARY_DIR}"
            "-DCONFIG=$<CONFIG>" "-DCTEST=${CMAKE_CTEST_COMMAND}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/Qualification.cmake"
        DEPENDS ${TE2350_QUALIFICATION_TARGETS} TE2350Antigravity_VST3
        USES_TERMINAL VERBATIM)
endif()

if(WIN32 AND CMAKE_SIZEOF_VOID_P EQUAL 8 AND TE2350_BUILD_GOLDEN_TESTS)
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "[Aa][Rr][Mm]|aarch64" OR CMAKE_GENERATOR_PLATFORM MATCHES "ARM")
        message(STATUS "Packaging currently supports Windows x64 only")
        return()
    endif()
    find_program(TE2350_DEPENDENCY_TOOL NAMES dumpbin llvm-objdump objdump REQUIRED)
    file(READ "${juce_SOURCE_DIR}/CMakeLists.txt" juce_project)
    string(REGEX MATCH "project\\(JUCE VERSION ([0-9.]+)" unused "${juce_project}")
    set(TE2350_JUCE_VERSION "${CMAKE_MATCH_1}")
    configure_file(cmake/PackageConfig.cmake.in PackageConfig.cmake @ONLY)
    add_test(NAME TE2350PackagingGateTest COMMAND "${CMAKE_COMMAND}"
        "-DPACKAGE_CONFIG=${CMAKE_CURRENT_BINARY_DIR}/PackageConfig.cmake"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/Tests/PackagingGateTest.cmake")
    set(TE2350_PACKAGE_NAME "TE-2350-Antigravity-${PROJECT_VERSION}-windows-x64")
    add_custom_target(package_te2350
        COMMAND "${CMAKE_COMMAND}"
            "-DPACKAGE_CONFIG=${CMAKE_CURRENT_BINARY_DIR}/PackageConfig.cmake"
            "-DCONFIG=$<CONFIG>"
            "-DVST3_BUNDLE=$<GENEX_EVAL:$<TARGET_PROPERTY:TE2350Antigravity_VST3,JUCE_PLUGIN_ARTEFACT_FILE>>"
            "-DVST3_BINARY=$<TARGET_FILE:TE2350Antigravity_VST3>"
            "-DSTANDALONE=$<TARGET_FILE:TE2350Antigravity_Standalone>"
            "-DSCANNER=$<TARGET_FILE:TE2350VST3LoadTest>"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/PackageRelease.cmake"
        DEPENDS ${TE2350_QUALIFICATION_TARGETS} TE2350Antigravity_VST3 TE2350Antigravity_Standalone
        USES_TERMINAL VERBATIM)
    # Retain the former command as a safe alias, without its old bypass.
    add_custom_target(TE2350PackageBeta DEPENDS package_te2350)
endif()
