cmake_minimum_required(VERSION 3.0.0)

project(browser VERSION 0.1 LANGUAGES CXX)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

find_package(Exiv2 REQUIRED)
find_package(Qt5 COMPONENTS Core Gui Widgets REQUIRED)
find_package(SodiumCxx REQUIRED)

if(APPLE)
    find_library(FWFoundation Foundation)
    find_library(FWAppKit AppKit)
    find_library(FWIOKit IOKit)
endif()

add_subdirectory(src/util)
add_subdirectory(src/3rdparty/qtc)
add_subdirectory(src/sodium-qt)
add_subdirectory(src/browser)
add_subdirectory(src/tools/setdatefrommeta)
