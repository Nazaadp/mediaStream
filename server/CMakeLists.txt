cmake_minimum_required(VERSION 3.20)
project(MediaStreamServer VERSION 2.0.0 LANGUAGES CXX)

# --- 1. Security & Standards ---
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Compiler Hardening (GCC/Clang)
# -Wall -Wextra: Enable warnings
# -Werror: Treat warnings as errors (Strict discipline)
# -fstack-protector-strong: Buffer overflow protection
if(MSVC)
    add_compile_options(/W4 /WX)
else()
    add_compile_options(-Wall -Wextra -Werror -fstack-protector-strong)
endif()

# --- 2. Dependencies (Conan) ---
# Assuming you use the conan_provider.cmake or conan_toolchain.cmake
find_package(LibtorrentRasterbar REQUIRED)
find_package(spdlog REQUIRED)
find_package(fmt REQUIRED)

# --- 3. Structure Definition ---
# Add the 'include' directory to the global search path
include_directories(include)

# Define the Main Library (The "Core")
add_library(MediaCore
    src/core/TorrentEngine.cpp
    # Add future source files here
)

# Link Dependencies to the Core
target_link_libraries(MediaCore PRIVATE 
    LibtorrentRasterbar::torrent-rasterbar
    spdlog::spdlog
    fmt::fmt
    oatpp::oatpp
    oatpp::oatpp-swagger
    oatpp::oatpp-sqlite
)

# Define the Executable (The "Runner")
add_executable(
    mediastream_server src/main.cpp
    src/api/HttpServer.cpp
)

# Link the Executable to the Core
target_link_libraries(mediastream_server PRIVATE
    MediaCore
    spdlog::spdlog
)