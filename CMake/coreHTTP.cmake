# This cmake file is used to include coreHTTP as static library.
set(CMAKE_corehttp_DIRECTORY ${REPO_ROOT_DIRECTORY}/libraries/coreHTTP)
include( ${CMAKE_corehttp_DIRECTORY}/httpFilePaths.cmake )

add_library( corehttp )

target_sources( corehttp
    PRIVATE
        ${HTTP_SOURCES}
    PUBLIC
        ${HTTP_INCLUDE_PUBLIC_DIRS}
)

target_include_directories( corehttp PUBLIC
                            ${HTTP_INCLUDE_PUBLIC_DIRS}
                            ${REPO_ROOT_DIRECTORY}/configs/corehttp
                            ${REPO_ROOT_DIRECTORY}/examples/demo_config # to get demo_config.h definition
)

# Suppress warnings for some Libraries
file(GLOB_RECURSE HTTP_WARNING_SUPPRESSED_SOURCES
    "${REPO_ROOT_DIRECTORY}/libraries/coreHTTP/source/dependency/3rdparty/llhttp/src/llhttp.c"
)

set_source_files_properties(
    ${HTTP_WARNING_SUPPRESSED_SOURCES}
    PROPERTIES
    COMPILE_FLAGS "-w"
)

### add linked library ###
list(
    APPEND app_example_lib
    corehttp
)
