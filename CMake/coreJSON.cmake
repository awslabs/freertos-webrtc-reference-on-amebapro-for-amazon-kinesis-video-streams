# This cmake file is used to include coreJSON as static library.
set(CMAKE_COREJSON_DIRECTORY ${REPO_ROOT_DIRECTORY}/libraries/coreJSON)
include( ${CMAKE_COREJSON_DIRECTORY}/jsonFilePaths.cmake )

add_library( corejson )

target_sources( corejson
    PRIVATE
        ${JSON_SOURCES}
    PUBLIC
        ${JSON_INCLUDE_PUBLIC_DIRS}
)

target_include_directories( corejson PUBLIC
                             ${JSON_INCLUDE_PUBLIC_DIRS} )

### add linked library ###
list(
    APPEND app_example_lib
    corejson
)

# # DCEP library source files.
# file(
#   GLOB
#   DCEP_SOURCES
#   "${CMAKE_DCEP_DIRECTORY}/source/*.c" )

# # DCEP library public include directories.
# set( DCEP_INCLUDE_PUBLIC_DIRS
#      "${CMAKE_DCEP_DIRECTORY}/source/include" )

# add_library( dcep
#              ${DCEP_SOURCES} )

# target_include_directories( dcep PRIVATE
#                             ${DCEP_INCLUDE_PUBLIC_DIRS} )

# target_link_libraries( dcep PRIVATE )

