cmake_minimum_required(VERSION 3.22)
if(NOT CONFIG STREQUAL "Release")
    message(FATAL_ERROR "Release qualification requires the Release configuration")
endif()
# Check the registered inventory before invoking CTest: disabling tests must fail.
execute_process(COMMAND "${CTEST}" --test-dir "${BUILD_DIR}" -C "${CONFIG}" --show-only=json-v1
    RESULT_VARIABLE inventory_result OUTPUT_VARIABLE inventory)
if(NOT inventory_result EQUAL 0)
    message(FATAL_ERROR "Cannot enumerate qualification tests")
endif()
set(required_tests
    TE2350MacroCalibrationTest TE2350PresetVoicingTest TE2350PresetWorkflowTest
    TE2350MusicalBehaviourTest TE2350FreezeConsistencyTest TE2350HostCompatibilityTest
    TE2350VST3LoadTest TE2350UIRenderTest TE2350ReleaseReadinessTest
    TE2350OfflineReferenceRender TE2350GoldenReferenceRender TE2350GoldenReferenceCompare
    TE2350CoreControlTest TE2350PluginSmokeTest TE2350CalibrationTest)
string(JSON test_count LENGTH "${inventory}" tests)
set(registered_tests "")
if(test_count GREATER 0)
    math(EXPR last_test "${test_count} - 1")
    foreach(index RANGE ${last_test})
        string(JSON test_name GET "${inventory}" tests ${index} name)
        list(APPEND registered_tests "${test_name}")
    endforeach()
endif()
foreach(test IN LISTS required_tests)
    if(NOT test IN_LIST registered_tests)
        message(FATAL_ERROR "Required qualification test missing: ${test}")
    endif()
endforeach()
execute_process(COMMAND "${CTEST}" --test-dir "${BUILD_DIR}" -C "${CONFIG}"
    --output-on-failure --no-tests=error --parallel 3 RESULT_VARIABLE result)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "Release qualification failed; packaging is forbidden")
endif()
