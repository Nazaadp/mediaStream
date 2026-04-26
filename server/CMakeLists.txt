cmake_minimum_required(VERSION 3.20)
project(MediaStreamServer VERSION 2.0.0 LANGUAGES CXX)

# --- 1. Security & Standards ---
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Compiler Hardening (GCC/Clang)
# Applied via target_compile_options securely below

# --- 2. Dependencies (Conan) ---
# Assuming you use the conan_provider.cmake or conan_toolchain.cmake
find_package(LibtorrentRasterbar REQUIRED)
find_package(spdlog REQUIRED)
find_package(fmt REQUIRED)
find_package(oatpp REQUIRED)

find_package(oatpp-sqlite REQUIRED)
find_package(oatpp-websocket REQUIRED)
find_package(CURL REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(SQLite3 REQUIRED)

# --- 3. Structure Definition ---
# Add the 'include' directory to the global search path
include_directories(include)

# Define the Main Library (The "Core")
add_library(MediaCore
    src/core/TorrentEngine.cpp
    src/database/Database.cpp
    src/services/YTSClient.cpp
    src/services/EZTVClient.cpp
    src/services/NyaaClient.cpp
    src/services/TorrentioClient.cpp
    src/services/ContentDiscoveryManager.cpp
    src/services/TMDBFetcher.cpp
    src/services/CinemetaClient.cpp
    src/services/TMDBCatalogClient.cpp
    # Add future source files here
)

target_include_directories(MediaCore PUBLIC include)

# Link Dependencies to the Core
target_link_libraries(MediaCore PUBLIC
    LibtorrentRasterbar::torrent-rasterbar
    spdlog::spdlog
    fmt::fmt
    CURL::libcurl
    nlohmann_json::nlohmann_json
    oatpp::oatpp
    oatpp::oatpp-sqlite
    oatpp::oatpp-websocket
    SQLite::SQLite3
)

# Define the Executable (The "Runner")
add_executable(mediastream_server
    src/main.cpp
    src/api/HttpServer.cpp
)

# Link the Executable to the Core
target_link_libraries(mediastream_server PRIVATE
    MediaCore
    spdlog::spdlog
    oatpp::oatpp             # Main/HttpServer

    oatpp::oatpp-sqlite      # Database
    oatpp::oatpp-websocket   # WebSockets
)

# 5. Compiler Flags
if (CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    # Base warnings and stack protection
    set(HARDENING_FLAGS "-Wall" "-Wextra" "-Wpedantic" "-Werror" "-fstack-protector-strong")

    # Position Independent Execution (ASLR) and Bounds Checking
    list(APPEND HARDENING_FLAGS "-fPIE" "-O2" "-D_FORTIFY_SOURCE=2" "-Wformat" "-Werror=format-security")

    target_compile_options(MediaCore PRIVATE ${HARDENING_FLAGS})
    target_compile_options(mediastream_server PRIVATE ${HARDENING_FLAGS})
    
    # Linker Hardening: Read-Only Relocations (RELRO) and Bind Now
    target_link_options(mediastream_server PRIVATE "-pie" "-Wl,-z,relro" "-Wl,-z,now" "-Wl,-z,noexecstack")
endif()

#    target_compile_options(mediastream_server PRIVATE -Wall -Wextra -Wpedantic -Werror)
#    target_compile_options(MediaCore PRIVATE -Wall -Wextra -Wpedantic -Werror)
#endif()

# 6. Standard
set_property(TARGET mediastream_server PROPERTY CXX_STANDARD 20)
set_property(TARGET MediaCore PROPERTY CXX_STANDARD 20)