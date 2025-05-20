cmake_minimum_required(VERSION 3.6)
include( ${REPO_ROOT_DIRECTORY}/project/realtek_amebapro2_webrtc_application/GCC-RELEASE/includepath.cmake )

project(usrsctp)

set(usrsctp usrsctp)

list(
    APPEND usrsctp_sources

###netinet
    ${REPO_ROOT_DIRECTORY}/libraries/usrsctp/usrsctplib/netinet/sctp_asconf.c
    ${REPO_ROOT_DIRECTORY}/libraries/usrsctp/usrsctplib/netinet/sctp_auth.c
    ${REPO_ROOT_DIRECTORY}/libraries/usrsctp/usrsctplib/netinet/sctp_bsd_addr.c
    ${REPO_ROOT_DIRECTORY}/libraries/usrsctp/usrsctplib/netinet/sctp_callout.c
    ${REPO_ROOT_DIRECTORY}/libraries/usrsctp/usrsctplib/netinet/sctp_cc_functions.c
    ${REPO_ROOT_DIRECTORY}/libraries/usrsctp/usrsctplib/netinet/sctp_crc32.c
    ${REPO_ROOT_DIRECTORY}/libraries/usrsctp/usrsctplib/netinet/sctp_indata.c
    ${REPO_ROOT_DIRECTORY}/libraries/usrsctp/usrsctplib/netinet/sctp_input.c
    ${REPO_ROOT_DIRECTORY}/libraries/usrsctp/usrsctplib/netinet/sctp_output.c
    ${REPO_ROOT_DIRECTORY}/libraries/usrsctp/usrsctplib/netinet/sctp_pcb.c
    ${REPO_ROOT_DIRECTORY}/libraries/usrsctp/usrsctplib/netinet/sctp_peeloff.c
    ${REPO_ROOT_DIRECTORY}/libraries/usrsctp/usrsctplib/netinet/sctp_sha1.c
    ${REPO_ROOT_DIRECTORY}/libraries/usrsctp/usrsctplib/netinet/sctp_ss_functions.c
    ${REPO_ROOT_DIRECTORY}/libraries/usrsctp/usrsctplib/netinet/sctp_sysctl.c
    ${REPO_ROOT_DIRECTORY}/libraries/usrsctp/usrsctplib/netinet/sctp_timer.c
    ${REPO_ROOT_DIRECTORY}/libraries/usrsctp/usrsctplib/netinet/sctp_userspace.c
    ${REPO_ROOT_DIRECTORY}/libraries/usrsctp/usrsctplib/netinet/sctp_usrreq.c
    ${REPO_ROOT_DIRECTORY}/libraries/usrsctp/usrsctplib/netinet/sctputil.c
###netinet6
    ${REPO_ROOT_DIRECTORY}/libraries/usrsctp/usrsctplib/netinet6/sctp6_usrreq.c

    ${REPO_ROOT_DIRECTORY}/libraries/usrsctp/usrsctplib/user_environment.c
    ${REPO_ROOT_DIRECTORY}/libraries/usrsctp/usrsctplib/user_mbuf.c
    ${REPO_ROOT_DIRECTORY}/libraries/usrsctp/usrsctplib/user_recv_thread.c
    ${REPO_ROOT_DIRECTORY}/libraries/usrsctp/usrsctplib/user_socket.c
)

list(
    APPEND usrsctp_include
    ${REPO_ROOT_DIRECTORY}/libraries/usrsctp/usrsctplib
    ${REPO_ROOT_DIRECTORY}/libraries/usrsctp/usrsctplib/netinet
    ${REPO_ROOT_DIRECTORY}/libraries/usrsctp/usrsctplib/netinet6
)

add_library( ${usrsctp} )

target_sources( ${usrsctp}
    PRIVATE
        ${usrsctp_sources}
        ${REPO_ROOT_DIRECTORY}/project/realtek_amebapro2_webrtc_application/src/amazon_kvs/lib_amazon/gcc_include
    PUBLIC
        ${inc_path}
        ${REPO_ROOT_DIRECTORY}/libraries/ambpro2_sdk/component/os/freertos/${freertos}/Source/portable/GCC/ARM_CM33_NTZ/non_secure
        ${usrsctp_include}
)

list(
    APPEND usrsctp_flags
    CONFIG_BUILD_RAM=1 
    CONFIG_BUILD_LIB=1 
    CONFIG_PLATFORM_8735B
    CONFIG_RTL8735B_PLATFORM=1
    CONFIG_SYSTEM_TIME64=1
    
    __Userspace__
    SCTP_SIMPLE_ALLOCATOR
    SCTP_PROCESS_LEVEL_LOCKS
    SCTP_USE_MBEDTLS_SHA1
    SCTP_USE_LWIP
    SCTP_USE_RTOS
    SCTP_DEBUG
    INET
    # INET6
    
    # HAVE_STDATOMIC_H
    HAVE_SA_LEN
    HAVE_SIN_LEN
    # HAVE_SIN6_LEN
    HAVE_SCONN_LEN

    KVS_PLAT_RTK_FREERTOS
)

target_compile_definitions( ${usrsctp} PUBLIC ${usrsctp_flags} )

target_include_directories( ${usrsctp}
    PRIVATE
        ${REPO_ROOT_DIRECTORY}/project/realtek_amebapro2_webrtc_application/src/amazon_kvs/lib_amazon/gcc_include
    PUBLIC
        ${inc_path}
        ${REPO_ROOT_DIRECTORY}/libraries/ambpro2_sdk/component/os/freertos/${freertos}/Source/portable/GCC/ARM_CM33_NTZ/non_secure
        ${usrsctp_include}
)

### add linked library ###
list(
    APPEND app_example_lib
    usrsctp
)
