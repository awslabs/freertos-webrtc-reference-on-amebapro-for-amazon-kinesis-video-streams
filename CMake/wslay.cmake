# This cmake file is used to include Wslay as static library.
include( ${REPO_ROOT_DIRECTORY}/project/realtek_amebapro2_webrtc_application/GCC-RELEASE/includepath.cmake )

file(
  GLOB
  WSLAY_SOURCE_FILES
  "${REPO_ROOT_DIRECTORY}/libraries/wslay/lib/*.c" )

configure_file(${REPO_ROOT_DIRECTORY}/libraries/wslay/lib/includes/wslay/wslayver.h.in
               ${REPO_ROOT_DIRECTORY}/libraries/wslay/lib/includes/wslay/wslayver.h @ONLY)

set( WSLAY_INCLUDE_DIRS
     "${REPO_ROOT_DIRECTORY}/libraries/wslay/lib/"
     "${REPO_ROOT_DIRECTORY}/libraries/wslay/lib/includes"
     "${REPO_ROOT_DIRECTORY}/libraries/wslay/lib/includes/wslay"
     "${REPO_ROOT_DIRECTORY}/configs/wslay" )

add_library( wslay )

target_sources( wslay
    PRIVATE
        ${WSLAY_SOURCE_FILES}
    PUBLIC
        ${WSLAY_INCLUDE_DIRS}
        ${inc_path}
        ${REPO_ROOT_DIRECTORY}/libraries/ambpro2_sdk/component/os/freertos/${freertos}/Source/portable/GCC/ARM_CM33_NTZ/non_secure
)

target_include_directories( wslay PUBLIC
                            ${WSLAY_INCLUDE_DIRS}
                            ${inc_path}
                            ${REPO_ROOT_DIRECTORY}/libraries/ambpro2_sdk/component/os/freertos/${freertos}/Source/portable/GCC/ARM_CM33_NTZ/non_secure )

target_compile_definitions( wslay PUBLIC HAVE_ARPA_INET_H )

### add linked library ###
list(
    APPEND app_example_lib
    wslay
)
