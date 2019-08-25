cmake_minimum_required(VERSION 3.0.0)

project(browser)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

add_subdirectory(src/browser)
add_subdirectory(src/tools/setdatefrommeta)
