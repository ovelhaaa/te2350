cmake_minimum_required(VERSION 3.22)
include("${PACKAGE_CONFIG}")
set(test_root "${BUILD_DIR}/PackagingGateTest")
file(REMOVE_RECURSE "${test_root}")
file(MAKE_DIRECTORY "${test_root}/tests" "${test_root}/dist")
set(names MacroCalibrationTest PresetVoicingTest PresetWorkflowTest MusicalBehaviourTest
    FreezeConsistencyTest HostCompatibilityTest VST3LoadTest UIRenderTest ReleaseReadinessTest
    OfflineReferenceRender GoldenReferenceRender GoldenReferenceCompare CoreControlTest
    PluginSmokeTest CalibrationTest)
set(test_file "${test_root}/tests/CTestTestfile.cmake")
file(WRITE "${test_file}" "")
foreach(name IN LISTS names)
    file(APPEND "${test_file}" "add_test(TE2350${name} \"${CMAKE_COMMAND}\" -E false)\n")
endforeach()
set(package_name "TE-2350-Antigravity-${VERSION}-windows-x64")
file(MAKE_DIRECTORY "${test_root}/dist/${package_name}")
file(WRITE "${test_root}/dist/${package_name}/stale.txt" "stale")
file(WRITE "${test_root}/dist/${package_name}.zip" "stale")
file(WRITE "${test_root}/dist/SHA256SUMS.txt" "stale")
file(WRITE "${test_root}/config.cmake"
    "include(\"${PACKAGE_CONFIG}\")\nset(BUILD_DIR \"${test_root}/tests\")\nset(OUTPUT_DIR \"${test_root}/dist\")\n")
execute_process(COMMAND "${CMAKE_COMMAND}" "-DPACKAGE_CONFIG=${test_root}/config.cmake"
    -DCONFIG=Release -P "${SOURCE_DIR}/cmake/PackageRelease.cmake"
    RESULT_VARIABLE failure OUTPUT_VARIABLE output ERROR_VARIABLE error)
file(WRITE "${test_root}/failed-qualification.log" "${output}${error}")
if(failure EQUAL 0 OR NOT error MATCHES "Release qualification failed")
    message(FATAL_ERROR "Packaging did not reject failed qualification: ${error}")
endif()
foreach(path IN ITEMS "${test_root}/dist/${package_name}" "${test_root}/dist/${package_name}.zip"
        "${test_root}/dist/SHA256SUMS.txt")
    if(EXISTS "${path}")
        message(FATAL_ERROR "Failed qualification left a distribution artifact: ${path}")
    endif()
endforeach()
file(WRITE "${test_file}" "add_test(TE2350PluginSmokeTest \"${CMAKE_COMMAND}\" -E true)\n")
execute_process(COMMAND "${CMAKE_COMMAND}" "-DBUILD_DIR=${test_root}/tests"
    "-DCTEST=${CTEST}" -DCONFIG=Release -P "${SOURCE_DIR}/cmake/Qualification.cmake"
    RESULT_VARIABLE failure OUTPUT_VARIABLE output ERROR_VARIABLE error)
if(failure EQUAL 0 OR NOT error MATCHES "Required qualification test missing")
    message(FATAL_ERROR "Qualification permitted a reduced inventory")
endif()
execute_process(COMMAND "${CMAKE_COMMAND}" "-DBUILD_DIR=${test_root}/tests"
    "-DCTEST=${CTEST}" -DCONFIG=Debug -P "${SOURCE_DIR}/cmake/Qualification.cmake"
    RESULT_VARIABLE failure OUTPUT_VARIABLE output ERROR_VARIABLE error)
if(failure EQUAL 0 OR NOT error MATCHES "requires the Release configuration")
    message(FATAL_ERROR "Qualification permitted a Debug release")
endif()
message(STATUS "Packaging gate passed: failed tests, missing inventory and Debug rejected")
