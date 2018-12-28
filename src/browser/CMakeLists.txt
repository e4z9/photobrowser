cmake_minimum_required(VERSION 3.0.0)
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

project(photobrowser VERSION 0.1 LANGUAGES CXX)

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt5Core)
find_package(Qt5Gui)
find_package(Qt5Widgets)

add_executable(${PROJECT_NAME} WIN32 MACOSX_BUNDLE
   "main.cpp"
   "browserwindow.cpp"
   "directorytree.cpp"
   "mediadirectorymodel.cpp"
   "filmrollview.cpp"
   "imageview.cpp"
   "../util/fileutil.cpp"
   "../util/metadatautil.cpp"
)
include_directories("../3rdparty/")

target_link_libraries(${PROJECT_NAME} Qt5::Core)
target_link_libraries(${PROJECT_NAME} Qt5::Gui)
target_link_libraries(${PROJECT_NAME} Qt5::Widgets)

target_link_libraries(${PROJECT_NAME} ${Exiv2_Library})
include_directories(${Exiv2_IncludePath})
