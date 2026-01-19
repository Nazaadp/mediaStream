cmake_minimum_required(VERSION 3.10)
project(MediaClient)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt5 REQUIRED COMPONENTS Widgets Network)

add_executable(client main.cpp mainwindow.cpp mainwindow.h)

target_link_libraries(client Qt5::Widgets Qt5::Network)