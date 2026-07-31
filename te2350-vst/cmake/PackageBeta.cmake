cmake_minimum_required(VERSION 3.22)

foreach(required_variable
        TE2350_VST3_BINARY
        TE2350_PACKAGE_OUTPUT_DIR
        TE2350_PACKAGE_NAME
        TE2350_RELEASE_VERSION
        TE2350_INSTALL_GUIDE
        TE2350_CHANGELOG
        TE2350_PLUGIN_README)
    if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
        message(FATAL_ERROR "PackageBeta.cmake requires ${required_variable}")
    endif()
endforeach()

if(NOT EXISTS "${TE2350_VST3_BINARY}")
    message(FATAL_ERROR "VST3 binary does not exist: ${TE2350_VST3_BINARY}")
endif()

get_filename_component(vst3_arch_directory "${TE2350_VST3_BINARY}" DIRECTORY)
get_filename_component(vst3_contents_directory "${vst3_arch_directory}" DIRECTORY)
get_filename_component(vst3_bundle "${vst3_contents_directory}" DIRECTORY)
get_filename_component(vst3_extension "${vst3_bundle}" LAST_EXT)
if(NOT vst3_extension STREQUAL ".vst3")
    message(FATAL_ERROR "Could not resolve the VST3 bundle from ${TE2350_VST3_BINARY}")
endif()

get_filename_component(package_output_base "${TE2350_PACKAGE_OUTPUT_DIR}" ABSOLUTE
                       BASE_DIR "${CMAKE_CURRENT_BINARY_DIR}")
set(package_root "${package_output_base}/${TE2350_PACKAGE_NAME}")
set(archive_path "${package_output_base}/${TE2350_PACKAGE_NAME}.zip")
set(checksum_path "${archive_path}.sha256")

file(MAKE_DIRECTORY "${package_output_base}")
file(REMOVE_RECURSE "${package_root}")
file(REMOVE "${archive_path}" "${checksum_path}")
file(MAKE_DIRECTORY "${package_root}/VST3")
file(COPY "${vst3_bundle}" DESTINATION "${package_root}/VST3")
file(COPY "${TE2350_INSTALL_GUIDE}" DESTINATION "${package_root}")
file(COPY "${TE2350_CHANGELOG}" DESTINATION "${package_root}")
file(COPY "${TE2350_PLUGIN_README}" DESTINATION "${package_root}")

if(NOT DEFINED TE2350_BUILD_CONFIGURATION OR TE2350_BUILD_CONFIGURATION STREQUAL "")
    set(TE2350_BUILD_CONFIGURATION "single-config")
endif()
if(NOT DEFINED TE2350_SYSTEM_PROCESSOR OR TE2350_SYSTEM_PROCESSOR STREQUAL "")
    set(TE2350_SYSTEM_PROCESSOR "unknown")
endif()

string(TIMESTAMP package_timestamp "%Y-%m-%dT%H:%M:%SZ" UTC)
file(SHA256 "${TE2350_VST3_BINARY}" vst3_binary_sha256)
file(WRITE "${package_root}/BUILD-MANIFEST.txt"
    "TE-2350 Antigravity\n"
    "Release: ${TE2350_RELEASE_VERSION}\n"
    "Built: ${package_timestamp}\n"
    "Configuration: ${TE2350_BUILD_CONFIGURATION}\n"
    "System: ${TE2350_SYSTEM_NAME}\n"
    "Architecture: ${TE2350_SYSTEM_PROCESSOR}\n"
    "VST3 binary SHA-256: ${vst3_binary_sha256}\n")

execute_process(
    COMMAND "${CMAKE_COMMAND}" -E tar cf "${archive_path}" --format=zip -- "${TE2350_PACKAGE_NAME}"
    WORKING_DIRECTORY "${package_output_base}"
    RESULT_VARIABLE archive_result
    OUTPUT_VARIABLE archive_output
    ERROR_VARIABLE archive_error)
if(NOT archive_result EQUAL 0)
    message(FATAL_ERROR "Could not create release archive: ${archive_error}${archive_output}")
endif()

file(SHA256 "${archive_path}" archive_sha256)
get_filename_component(archive_filename "${archive_path}" NAME)
file(WRITE "${checksum_path}" "${archive_sha256}  ${archive_filename}\n")

message(STATUS "Created ${archive_path}")
message(STATUS "SHA-256 ${archive_sha256}")
