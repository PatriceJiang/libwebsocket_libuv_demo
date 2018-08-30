cmake_minimum_required(VERSION 2.8)

project("libwebsocket_uv" CXX)

set(CMAKE_CXX_STANDARD 11)
#set(LIBS_ROOT "${CMAKE_SOURCE_DIR}/libs")
set(LIBS_ROOT "${CMAKE_SOURCE_DIR}/libs")

message(INFO "--- lib root ${LIBS_ROOT}")

set(ws_root "websockets-master")
#set(ws_root "websockets")

include_directories(${LIBS_ROOT}/${ws_root}/include/win32)
include_directories(${LIBS_ROOT}/uv/include)
include_directories(${LIBS_ROOT}/openssl/include/win32)

link_directories(${LIBS_ROOT}/openssl/prebuilt/win32)
link_directories(${LIBS_ROOT}/${ws_root}/prebuilt/win32)
link_directories(${LIBS_ROOT}/uv/prebuilt/win32)

find_library(SSL STATIC libssl.lib PATHS ${LIBS_ROOT}/openssl/prebuilt/win32)
find_library(CRYPTO STATIC libcrypto.lib PATHS ${LIBS_ROOT}/openssl/prebuilt/win32)
find_library(WEBSOCKET STATIC websockets  PATHS ${LIBS_ROOT}/${ws_root}/prebuilt/win32)
find_library(UV STATIC uv_a PATHS ${LIBS_ROOT}/uv/prebuilt/win32)

set(SRC_LIST 
Main.cpp 
url.cpp
)

add_executable(test ${SRC_LIST})
target_link_libraries(test ${SSL} ${CRYPTO} ${UV} ${WEBSOCKET})

add_custom_command(TARGET test POST_BUILD COMMAND "${CMAKE_COMMAND}" 
    -E copy_if_different 
    "${LIBS_ROOT}/openssl/prebuilt/win32/libcrypto-1_1.dll"     
    $<TARGET_FILE_DIR:test>
)

add_custom_command(TARGET test POST_BUILD COMMAND "${CMAKE_COMMAND}" 
    -E copy_if_different 
    "${LIBS_ROOT}/openssl/prebuilt/win32/libssl-1_1.dll"     
    $<TARGET_FILE_DIR:test>
)

add_custom_command(TARGET test POST_BUILD COMMAND "${CMAKE_COMMAND}" 
    -E copy_if_different 
    "${LIBS_ROOT}/${ws_root}/prebuilt/win32/websockets.dll"     
    $<TARGET_FILE_DIR:test>
)

add_custom_command(TARGET test POST_BUILD COMMAND "${CMAKE_COMMAND}" 
    -E copy_if_different 
    "${LIBS_ROOT}/uv/prebuilt/win32/uv.dll"      
    $<TARGET_FILE_DIR:test>
)