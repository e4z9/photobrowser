cmake_minimum_required(VERSION 3.0.0)

project(browser)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

find_package(Exiv2 REQUIRED)
find_package(Qt5 COMPONENTS Core Gui REQUIRED)

if(APPLE)
    find_library(FWFoundation Foundation)
    find_library(FWAppKit AppKit)
endif()

add_subdirectory(src/browser)
add_subdirectory(src/tools/setdatefrommeta)
