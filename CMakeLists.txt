cmake_minimum_required(VERSION 3.0.0)

project(browser VERSION 0.1 LANGUAGES CXX)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

find_package(exiv2 REQUIRED)

find_package(QT NAMES Qt6 Qt5 COMPONENTS Core REQUIRED)
set(Qt "Qt${QT_VERSION_MAJOR}")
find_package(${Qt} COMPONENTS Core Gui Widgets Concurrent REQUIRED)

if(APPLE)
    find_library(FWFoundation Foundation)
    find_library(FWAppKit AppKit)
    find_library(FWIOKit IOKit)
endif()

if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/src/3rdparty/sodium-cxx/CMakeLists.txt)
  add_subdirectory(src/3rdparty/sodium-cxx)
endif()
find_package(SodiumCxx REQUIRED)

add_subdirectory(src/util)
add_subdirectory(src/3rdparty/qtc)
add_subdirectory(src/3rdparty/sodium-qt)
add_subdirectory(src/browser)
add_subdirectory(src/tools/setdatefrommeta)
