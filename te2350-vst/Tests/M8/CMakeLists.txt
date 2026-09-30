cmake_minimum_required(VERSION 3.22)
project(TE2350M8Core LANGUAGES C CXX)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
include(CTest)
get_filename_component(ROOT "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)
add_library(m8_core STATIC
    "${ROOT}/src/te2350.c" "${ROOT}/src/dsp_delay.c"
    "${ROOT}/src/dsp_filters.c" "${ROOT}/src/dsp_fdn.c"
    "${ROOT}/src/dsp_modulation.c" "${ROOT}/src/dsp_pitch.c"
    "${ROOT}/src/dsp_melody.c")
set_target_properties(m8_core PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_compile_definitions(m8_core PUBLIC TE_MAIN_DELAY_SIZE=524288)
if(UNIX)
    target_link_libraries(m8_core PUBLIC m)
endif()
add_executable(M8CoreTest ../M8CoreTest.cpp)
target_link_libraries(M8CoreTest PRIVATE m8_core)
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND (UNIX OR MINGW))
    target_compile_definitions(M8CoreTest PRIVATE M8_ALLOCATION_AUDIT)
    target_link_options(M8CoreTest PRIVATE
        -Wl,--wrap=malloc -Wl,--wrap=calloc -Wl,--wrap=realloc -Wl,--wrap=free)
endif()
add_test(NAME M8CoreTest COMMAND M8CoreTest)
add_library(M8Render SHARED ../M8Render.c)
target_compile_definitions(M8Render PRIVATE M8_ADAPTIVE)
target_link_libraries(M8Render PRIVATE m8_core)
