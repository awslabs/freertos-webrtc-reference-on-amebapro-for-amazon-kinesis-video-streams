cmake_minimum_required(VERSION 3.6.3)

project(FreeRTOSWebRTCMasterApplication VERSION 0.0.1 LANGUAGES C)

set(REPO_ROOT_DIRECTORY ${repo_root})

option(ENABLE_STREAMING_LOOPBACK "Loopback the received frames to the remote peer" OFF)

# Option to control linking with usrsctp
option(BUILD_USRSCTP_LIBRARY "Enable linking with usrsctp" ON)

# Option to enable metric logging
option(METRIC_PRINT_ENABLED "Enable Metric print logging" OFF)

file(
  GLOB
  WEBRTC_APPLICATION_MASTER_SOURCE_FILES
  "${REPO_ROOT_DIRECTORY}/examples/master/*.c"
  "${REPO_ROOT_DIRECTORY}/examples/app_common/*.c"
  "${REPO_ROOT_DIRECTORY}/examples/peer_connection/*.c"
  "${REPO_ROOT_DIRECTORY}/examples/peer_connection/peer_connection_codec_helper/*.c"
  "${REPO_ROOT_DIRECTORY}/examples/signaling_controller/*.c"
  "${REPO_ROOT_DIRECTORY}/examples/network_transport/*.c"
  "${REPO_ROOT_DIRECTORY}/examples/network_transport/tcp_sockets_wrapper/ports/lwip/*.c"
  "${REPO_ROOT_DIRECTORY}/examples/network_transport/udp_sockets_wrapper/ports/lwip/*.c"
  "${REPO_ROOT_DIRECTORY}/examples/networking/corehttp_helper/*.c"
  "${REPO_ROOT_DIRECTORY}/examples/networking/networking_utils/*.c"
  "${REPO_ROOT_DIRECTORY}/examples/networking/wslay_helper/*.c"
  "${REPO_ROOT_DIRECTORY}/examples/message_queue/*.c"
  "${REPO_ROOT_DIRECTORY}/examples/base64/mbedtls/*.c"
  "${REPO_ROOT_DIRECTORY}/examples/sdp_controller/*.c"
  "${REPO_ROOT_DIRECTORY}/examples/string_utils/*.c"
  "${REPO_ROOT_DIRECTORY}/examples/ice_controller/*.c"
  "${REPO_ROOT_DIRECTORY}/examples/timer_controller/*.c"
  "${REPO_ROOT_DIRECTORY}/examples/app_media_source/*.c"
  "${REPO_ROOT_DIRECTORY}/examples/app_media_source/port/ameba_pro2/*.c" )

set( WEBRTC_APPLICATION_MASTER_INCLUDE_DIRS
     "${REPO_ROOT_DIRECTORY}/examples/master"
     "${REPO_ROOT_DIRECTORY}/examples/app_common"
     "${REPO_ROOT_DIRECTORY}/examples/peer_connection"
     "${REPO_ROOT_DIRECTORY}/examples/peer_connection/peer_connection_codec_helper"
     "${REPO_ROOT_DIRECTORY}/examples/peer_connection/peer_connection_codec_helper/include"
     "${REPO_ROOT_DIRECTORY}/examples/signaling_controller"
     "${REPO_ROOT_DIRECTORY}/examples/network_transport"
     "${REPO_ROOT_DIRECTORY}/examples/network_transport/tcp_sockets_wrapper/include"
     "${REPO_ROOT_DIRECTORY}/examples/network_transport/udp_sockets_wrapper/include"
     "${REPO_ROOT_DIRECTORY}/examples/networking"
     "${REPO_ROOT_DIRECTORY}/examples/networking/corehttp_helper"
     "${REPO_ROOT_DIRECTORY}/examples/networking/wslay_helper"
     "${REPO_ROOT_DIRECTORY}/examples/networking/networking_utils"
     "${REPO_ROOT_DIRECTORY}/examples/logging"
     "${REPO_ROOT_DIRECTORY}/examples/message_queue"
     "${REPO_ROOT_DIRECTORY}/examples/base64"
     "${REPO_ROOT_DIRECTORY}/examples/sdp_controller"
     "${REPO_ROOT_DIRECTORY}/examples/string_utils"
     "${REPO_ROOT_DIRECTORY}/examples/ice_controller"
     "${REPO_ROOT_DIRECTORY}/examples/timer_controller"
     "${REPO_ROOT_DIRECTORY}/examples/app_media_source"
     "${REPO_ROOT_DIRECTORY}/examples/app_media_source/port/ameba_pro2"
     "${REPO_ROOT_DIRECTORY}/examples/demo_config" )

if( BUILD_USRSCTP_LIBRARY )
     file( GLOB USRSCTP_SRC_FILES "${REPO_ROOT_DIRECTORY}/examples/libusrsctp/*.c" )
     list( APPEND WEBRTC_APPLICATION_MASTER_SOURCE_FILES ${USRSCTP_SRC_FILES} )
     list( APPEND WEBRTC_APPLICATION_MASTER_INCLUDE_DIRS "${REPO_ROOT_DIRECTORY}/examples/libusrsctp" )
endif()
 
if( METRIC_PRINT_ENABLED )
     file(GLOB METRIC_SRC_FILES "${REPO_ROOT_DIRECTORY}/examples/metric/*.c")
     list(APPEND WEBRTC_APPLICATION_MASTER_SOURCE_FILES ${METRIC_SRC_FILES})
     list( APPEND WEBRTC_APPLICATION_MASTER_INCLUDE_DIRS "${REPO_ROOT_DIRECTORY}/examples/metric" )
endif()
 
# Include dependencies
# Include coreHTTP
include( ${REPO_ROOT_DIRECTORY}/CMake/coreHTTP.cmake )

# Include sigV4
include( ${REPO_ROOT_DIRECTORY}/CMake/sigV4.cmake )

## Include coreJSON
include( ${REPO_ROOT_DIRECTORY}/CMake/coreJSON.cmake )

## Include Signaling
include( ${REPO_ROOT_DIRECTORY}/CMake/signaling.cmake )

# Suppress warnings for some Libraries
file(GLOB_RECURSE WARNING_SUPPRESSED_SOURCES
    "${REPO_ROOT_DIRECTORY}/libraries/ambpro2_sdk/*.c"
    "${REPO_ROOT_DIRECTORY}/libraries/usrsctp/*.c"
)

set_source_files_properties(
    ${WARNING_SUPPRESSED_SOURCES}
    PROPERTIES
    COMPILE_FLAGS "-w"
)

# Include wslay
include( ${REPO_ROOT_DIRECTORY}/CMake/wslay.cmake )

# Include SDP
include( ${REPO_ROOT_DIRECTORY}/CMake/sdp.cmake )

# Include STUN
include( ${REPO_ROOT_DIRECTORY}/CMake/stun.cmake )

# Include RTP
include( ${REPO_ROOT_DIRECTORY}/CMake/rtp.cmake )

# Include RTCP
include( ${REPO_ROOT_DIRECTORY}/CMake/rtcp.cmake )

# Include ICE
include( ${REPO_ROOT_DIRECTORY}/CMake/ice.cmake )

# Include libsrtp
include( ${REPO_ROOT_DIRECTORY}/CMake/libsrtp.cmake )

set( webrtc_master_demo_src
     ${WEBRTC_APPLICATION_MASTER_SOURCE_FILES} )

set( webrtc_master_demo_include
     ${WEBRTC_APPLICATION_MASTER_INCLUDE_DIRS} )

if(BUILD_USRSCTP_LIBRARY)
     # Include DCEP
     include( ${REPO_ROOT_DIRECTORY}/CMake/dcep.cmake )
     # Include usrsctp
     include( ${REPO_ROOT_DIRECTORY}/CMake/usrsctp.cmake )

     list(
          APPEND app_flags
          ENABLE_SCTP_DATA_CHANNEL=1
     )
else()
     list(
          APPEND app_flags
          ENABLE_SCTP_DATA_CHANNEL=0
     )
endif()

if(METRIC_PRINT_ENABLED)
     list(
          APPEND app_flags
          METRIC_PRINT_ENABLED=1
     )
else()
     list(
          APPEND app_flags
          METRIC_PRINT_ENABLED=0
     )
endif()

# Set more strict rules to application code only
set_source_files_properties(
     ${WEBRTC_APPLICATION_MASTER_SOURCE_FILES}
     ${WEBRTC_APPLICATION_MASTER_INCLUDE_DIRS}
     PROPERTIES
     COMPILE_FLAGS "-Werror"
)

if( ENABLE_STREAMING_LOOPBACK )
     list(
          APPEND app_flags
          ENABLE_STREAMING_LOOPBACK
     )
endif()
