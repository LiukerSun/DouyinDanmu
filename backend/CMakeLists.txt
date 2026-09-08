cmake_minimum_required(VERSION 3.20)
project(DouyinDanmu VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Generate compile_commands.json for IDE support
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Windows-specific definitions
if(WIN32)
    add_definitions(-D_WIN32 -DWIN32 -D_WINDOWS)
    # Suppress common Windows warnings
    add_definitions(-D_CRT_SECURE_NO_WARNINGS -D_WINSOCK_DEPRECATED_NO_WARNINGS)
endif()

# ==============================================================================
# FetchContent dependencies
# ==============================================================================
include(FetchContent)

# --- nlohmann/json ---
FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG        v3.11.3
    GIT_SHALLOW    TRUE
)

# --- toml++ ---
FetchContent_Declare(
    tomlplusplus
    GIT_REPOSITORY https://github.com/marzer/tomlplusplus.git
    GIT_TAG        v3.4.0
    GIT_SHALLOW    TRUE
)

# --- spdlog ---
FetchContent_Declare(
    spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog.git
    GIT_TAG        v1.13.0
    GIT_SHALLOW    TRUE
)

# --- zlib ---
FetchContent_Declare(
    zlib
    GIT_REPOSITORY https://github.com/madler/zlib.git
    GIT_TAG        v1.3.1
    GIT_SHALLOW    TRUE
)

# --- SQLite (download amalgamation) ---
FetchContent_Declare(
    sqlite3
    URL      https://www.sqlite.org/2024/sqlite-amalgamation-3450100.zip
)

# --- cpp-httplib ---
FetchContent_Declare(
    cpp_httplib
    GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git
    GIT_TAG        v0.15.3
    GIT_SHALLOW    TRUE
)

# --- Protobuf (optional - use system or skip) ---
# FetchContent_Declare(
#     protobuf
#     GIT_REPOSITORY https://github.com/protocolbuffers/protobuf.git
#     GIT_TAG        v25.1
#     GIT_SHALLOW    TRUE
# )

# --- IXWebSocket ---
FetchContent_Declare(
    ixwebsocket
    GIT_REPOSITORY https://github.com/machinezone/IXWebSocket.git
    GIT_TAG        v11.4.5
    GIT_SHALLOW    TRUE
)

# --- QuickJS ---
# Using local QuickJS source (downloaded manually)
set(QUICKJS_LOCAL_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/quickjs")

# ==============================================================================
# Configure and fetch all dependencies
# ==============================================================================

# Set options before making dependencies available
set(TOMLPP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(TOMLPP_INSTALL OFF CACHE BOOL "" FORCE)
set(SPDLOG_INSTALL OFF CACHE BOOL "" FORCE)
set(SPDLOG_BUILD_EXAMPLE OFF CACHE BOOL "" FORCE)
set(BUILD_TESTING OFF CACHE BOOL "" FORCE)

# IXWebSocket options
set(USE_TLS ON CACHE BOOL "" FORCE)
set(USE_OPEN_SSL ON CACHE BOOL "" FORCE)
set(USE_ZLIB OFF CACHE BOOL "" FORCE)
set(USE_WS OFF CACHE BOOL "" FORCE)
set(USE_TEST OFF CACHE BOOL "" FORCE)
set(USE_MSGPACK OFF CACHE BOOL "" FORCE)

# cpp-httplib options
set(HTTPLIB_REQUIRE_OPENSSL OFF CACHE BOOL "" FORCE)
set(HTTPLIB_USE_OPENSSL_IF_AVAILABLE OFF CACHE BOOL "" FORCE)

# Make all dependencies available
FetchContent_MakeAvailable(
    nlohmann_json
    tomlplusplus
    spdlog
    zlib
    sqlite3
    cpp_httplib
    ixwebsocket
)

# ==============================================================================
# SQLite3 static library (from amalgamation)
# ==============================================================================
add_library(sqlite3_static STATIC
    ${sqlite3_SOURCE_DIR}/sqlite3.c
)
target_include_directories(sqlite3_static PUBLIC ${sqlite3_SOURCE_DIR})
target_compile_definitions(sqlite3_static PRIVATE
    SQLITE_THREADSAFE=1
    SQLITE_ENABLE_FTS5
    SQLITE_ENABLE_JSON1
)
if(WIN32)
    target_link_libraries(sqlite3_static PRIVATE ws2_32)
endif()

# ==============================================================================
# QuickJS (compile from source as static library)
# ==============================================================================
# QuickJS needs special handling - compile key source files directly
set(QUICKJS_DIR ${QUICKJS_LOCAL_DIR})

# Find all .c files in QuickJS source directory
file(GLOB QUICKJS_SOURCES
    ${QUICKJS_DIR}/quickjs.c
    ${QUICKJS_DIR}/cutils.c
    ${QUICKJS_DIR}/libunicode.c
    ${QUICKJS_DIR}/libregexp.c
    ${QUICKJS_DIR}/libbf.c
    ${QUICKJS_DIR}/quickjs-libc.c
)

# Only build if source files exist
if(EXISTS "${QUICKJS_DIR}/quickjs.c")
    add_library(quickjs_static STATIC ${QUICKJS_SOURCES})
    target_include_directories(quickjs_static PUBLIC ${QUICKJS_DIR})
    target_compile_definitions(quickjs_static PRIVATE
        CONFIG_VERSION="${PROJECT_VERSION}"
        _GNU_SOURCE
    )
    if(WIN32)
        target_compile_definitions(quickjs_static PRIVATE
            WIN32_LEAN_AND_MEAN
        )
    endif()
    set(QUICKJS_AVAILABLE TRUE)
    message(STATUS "QuickJS source found, building from: ${QUICKJS_DIR}")
else()
    # Create a stub library if QuickJS source is not available
    set(QUICKJS_AVAILABLE FALSE)
    message(WARNING "QuickJS source not found at ${QUICKJS_DIR}, creating stub")
endif()

# ==============================================================================
# Protobuf (optional - for douyin.proto compilation)
# ==============================================================================
# Try to find system protobuf
find_package(Protobuf QUIET)
if(Protobuf_FOUND AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/proto/douyin.proto")
    set(PROTO_OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/proto_gen")
    file(MAKE_DIRECTORY ${PROTO_OUTPUT_DIR})

    set(PROTO_FILE "${CMAKE_CURRENT_SOURCE_DIR}/proto/douyin.proto")
    set(PROTO_SRCS "${PROTO_OUTPUT_DIR}/douyin.pb.cc")
    set(PROTO_HDRS "${PROTO_OUTPUT_DIR}/douyin.pb.h")

    # Custom command to generate protobuf sources
    add_custom_command(
        OUTPUT ${PROTO_SRCS} ${PROTO_HDRS}
        COMMAND ${Protobuf_PROTOC_EXECUTABLE}
            --cpp_out=${PROTO_OUTPUT_DIR}
            --proto_path=${CMAKE_CURRENT_SOURCE_DIR}/proto
            ${PROTO_FILE}
        DEPENDS ${PROTO_FILE}
        COMMENT "Compiling douyin.proto -> ${PROTO_OUTPUT_DIR}"
        VERBATIM
    )
    message(STATUS "Protobuf found, will compile douyin.proto")
else()
    message(STATUS "Protobuf not found or proto file missing, skipping proto compilation")
    set(PROTO_SRCS "")
    set(PROTO_HDRS "")
endif()

# ==============================================================================
# OpenSSL (for cpp-httplib and IXWebSocket TLS)
# ==============================================================================
# Try to find OpenSSL, or use manually specified paths
if(NOT DEFINED OPENSSL_ROOT_DIR)
    set(OPENSSL_ROOT_DIR "C:/OpenSSL-dev")
endif()

find_package(OpenSSL QUIET)
if(NOT OpenSSL_FOUND AND EXISTS "C:/OpenSSL-dev/include/openssl/ssl.h")
    # Manually set OpenSSL paths
    set(OpenSSL_FOUND TRUE)
    set(OPENSSL_INCLUDE_DIR "C:/OpenSSL-dev/include")
    set(OPENSSL_CRYPTO_LIBRARY "C:/OpenSSL-dev/lib/libcrypto.a")
    set(OPENSSL_SSL_LIBRARY "C:/OpenSSL-dev/lib/libssl.a")
    set(OPENSSL_LIBRARIES ${OPENSSL_SSL_LIBRARY} ${OPENSSL_CRYPTO_LIBRARY})
    message(STATUS "OpenSSL found (manual): ${OPENSSL_INCLUDE_DIR}")
elseif(OpenSSL_FOUND)
    message(STATUS "OpenSSL found: ${OPENSSL_VERSION}")
else()
    message(WARNING "OpenSSL not found - TLS features may be limited")
endif()

# ==============================================================================
# douyin_core static library
# ==============================================================================
set(DOUYIN_CORE_SOURCES
    src/server/http_server.cpp
    src/server/ws_server.cpp
    src/server/static_file.cpp
    src/capture/capture_engine.cpp
    src/capture/douyin_client.cpp
    src/capture/auth_manager.cpp
    src/capture/message_parser.cpp
    src/capture/signature.cpp
    src/capture/room_manager.cpp
    src/storage/database.cpp
    src/storage/message_store.cpp
    src/models/room.cpp
    src/models/message.cpp
    src/models/config.cpp
    src/utils/logger.cpp
    src/utils/timer.cpp
    src/utils/gzip_utils.cpp
)

# Add protobuf sources if available
if(PROTO_SRCS)
    list(APPEND DOUYIN_CORE_SOURCES ${PROTO_SRCS})
endif()

add_library(douyin_core STATIC ${DOUYIN_CORE_SOURCES})

target_include_directories(douyin_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# Add protobuf generated headers directory
if(EXISTS "${PROTO_OUTPUT_DIR}")
    target_include_directories(douyin_core PUBLIC ${PROTO_OUTPUT_DIR})
endif()

# Link dependencies
target_link_libraries(douyin_core PUBLIC
    nlohmann_json::nlohmann_json
    tomlplusplus::tomlplusplus
    spdlog::spdlog
    zlibstatic
    sqlite3_static
    httplib::httplib
    ixwebsocket::ixwebsocket
)

# Link protobuf if available
if(Protobuf_FOUND)
    target_link_libraries(douyin_core PUBLIC ${Protobuf_LIBRARIES})
    target_include_directories(douyin_core PUBLIC ${Protobuf_INCLUDE_DIRS})
endif()

# Link QuickJS if available
if(QUICKJS_AVAILABLE)
    target_link_libraries(douyin_core PUBLIC quickjs_static)
    target_compile_definitions(douyin_core PUBLIC HAS_QUICKJS=1)
endif()

# Link OpenSSL if available
if(OpenSSL_FOUND)
    target_link_libraries(douyin_core PUBLIC OpenSSL::SSL OpenSSL::Crypto)
endif()

# Windows-specific link libraries
if(WIN32)
    target_link_libraries(douyin_core PUBLIC
        ws2_32
        crypt32
    )
endif()

# ==============================================================================
# Main executable
# ==============================================================================
add_executable(douyin-danmu src/main.cpp)

target_link_libraries(douyin-danmu PRIVATE
    douyin_core
)

# Set the output directory
set_target_properties(douyin-danmu PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

# Copy config.toml.example to build directory
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/config.toml.example
    ${CMAKE_BINARY_DIR}/bin/config.toml.example
    COPYONLY
)

# ==============================================================================
# Print summary
# ==============================================================================
message(STATUS "")
message(STATUS "=== DouyinDanmu Build Configuration ===")
message(STATUS "  Version:     ${PROJECT_VERSION}")
message(STATUS "  C++ Std:     ${CMAKE_CXX_STANDARD}")
message(STATUS "  Platform:    ${CMAKE_SYSTEM_NAME}")
message(STATUS "  Compiler:    ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "  OpenSSL:     ${OPENSSL_VERSION}")
message(STATUS "  Protobuf:    ${Protobuf_FOUND}")
message(STATUS "  QuickJS:     ${QUICKJS_AVAILABLE}")
message(STATUS "========================================")
message(STATUS "")
