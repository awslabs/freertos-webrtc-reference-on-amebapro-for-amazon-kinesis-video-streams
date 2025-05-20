cmake_minimum_required(VERSION 3.6)
include( ${REPO_ROOT_DIRECTORY}/project/realtek_amebapro2_webrtc_application/GCC-RELEASE/includepath.cmake )

project(libsrtp)
set(libsrtp libsrtp)

# libsrtp library source files.
file(GLOB LIBSRTP_GLOB_SOURCES
          ${REPO_ROOT_DIRECTORY}/libraries/libsrtp/crypto/kernel/*.c
          ${REPO_ROOT_DIRECTORY}/libraries/libsrtp/crypto/math/*.c
          ${REPO_ROOT_DIRECTORY}/libraries/libsrtp/crypto/replay/*.c
          ${REPO_ROOT_DIRECTORY}/libraries/libsrtp/srtp/*.c )
set(LIBSRTP_SOURCES
          ${LIBSRTP_GLOB_SOURCES}
          "${REPO_ROOT_DIRECTORY}/libraries/libsrtp/crypto/cipher/aes.c"
          "${REPO_ROOT_DIRECTORY}/libraries/libsrtp/crypto/cipher/aes_gcm_mbedtls.c"
          "${REPO_ROOT_DIRECTORY}/libraries/libsrtp/crypto/cipher/aes_icm_mbedtls.c"
          "${REPO_ROOT_DIRECTORY}/libraries/libsrtp/crypto/cipher/cipher.c"
          "${REPO_ROOT_DIRECTORY}/libraries/libsrtp/crypto/cipher/cipher_test_cases.c"
          "${REPO_ROOT_DIRECTORY}/libraries/libsrtp/crypto/cipher/null_cipher.c"
          "${REPO_ROOT_DIRECTORY}/libraries/libsrtp/crypto/hash/auth_test_cases.c"
          "${REPO_ROOT_DIRECTORY}/libraries/libsrtp/crypto/hash/auth.c"
          "${REPO_ROOT_DIRECTORY}/libraries/libsrtp/crypto/hash/hmac.c"
          "${REPO_ROOT_DIRECTORY}/libraries/libsrtp/crypto/hash/hmac_mbedtls.c"
          "${REPO_ROOT_DIRECTORY}/libraries/libsrtp/crypto/hash/null_auth.c"
          "${REPO_ROOT_DIRECTORY}/libraries/libsrtp/crypto/hash/sha1.c"
          )

# libsrtp library Public Include directories.
set( LIBSRTP_INCLUDE_PUBLIC_DIRS
     "${REPO_ROOT_DIRECTORY}/examples/libsrtp"
     "${REPO_ROOT_DIRECTORY}/libraries/libsrtp/include"
     "${REPO_ROOT_DIRECTORY}/libraries/libsrtp/crypto/include" )

add_library( ${libsrtp} )

target_sources( ${libsrtp}
    PRIVATE
        ${LIBSRTP_SOURCES}
    PUBLIC
        ${LIBSRTP_INCLUDE_PUBLIC_DIRS}
        ${inc_path}
        ${REPO_ROOT_DIRECTORY}/libraries/ambpro2_sdk/component/os/freertos/${freertos}/Source/portable/GCC/ARM_CM33_NTZ/non_secure
)

list(
    APPEND libsrtp_flags
    HAVE_CONFIG_H
)

target_compile_definitions(${libsrtp} PRIVATE ${libsrtp_flags})

target_include_directories(
    ${libsrtp}
    PUBLIC
    ${LIBSRTP_INCLUDE_PUBLIC_DIRS}
    ${inc_path}
    ${sdk_root}/component/os/freertos/${freertos}/Source/portable/GCC/ARM_CM33_NTZ/non_secure
)

### add linked library ###
list(
    APPEND app_example_lib
    ${libsrtp}
)
