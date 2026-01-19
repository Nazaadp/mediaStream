cmake_minimum_required(VERSION 3.10)
project(MediaServer)

set(CMAKE_CXX_STANDARD 17)

# Encontrar paquetes
find_package(Boost REQUIRED COMPONENTS system)
find_package(OpenSSL REQUIRED)
# libtorrent a veces requiere configuración manual si pkg-config falla, 
# pero intentaremos el enlace estándar.
find_library(LIBTORRENT_LIB torrent-rasterbar)

add_executable(server main.cpp)

target_link_libraries(server 
    ${Boost_LIBRARIES} 
    ${LIBTORRENT_LIB} 
    OpenSSL::SSL 
    OpenSSL::Crypto
    pthread
)