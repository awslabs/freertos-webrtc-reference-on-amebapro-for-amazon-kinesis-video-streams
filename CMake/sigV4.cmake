# This cmake file is used to include libwebsockets as static library.
set(CMAKE_SIGV4_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}/../libraries/crypto/SigV4-for-AWS-IoT-embedded-sdk)
include( ${CMAKE_SIGV4_DIRECTORY}/sigv4FilePaths.cmake )

add_library( sigv4_config
             ${SIGV4_SOURCES} )

target_include_directories( sigv4_config PRIVATE
                            ${REPO_ROOT_DIRECTORY}/configs/sigv4
                            ${SIGV4_INCLUDE_PUBLIC_DIRS} )