cmake_minimum_required(VERSION 3.0.0)
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

project(photobrowser VERSION 0.1 LANGUAGES CXX)

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt5Core)
find_package(Qt5Gui)
find_package(Qt5Widgets)

set(UTIL_SOURCES
    "../util/fileutil.cpp"
    "../util/metadatautil.cpp"
)

set(QTC_UTILS_SOURCES
    "../3rdparty/qtc/progressindicator.cpp"
    "../3rdparty/qtc/runextensions.cpp"
    "../3rdparty/qtc/utils.qrc"
)
add_compile_definitions(QTCREATOR_UTILS_EXPORT=)

add_executable(${PROJECT_NAME} WIN32 MACOSX_BUNDLE
   "main.cpp"
   "browserwindow.cpp"
   "directorytree.cpp"
   "mediadirectorymodel.cpp"
   "filmrollview.cpp"
   "imageview.cpp"
   ${UTIL_SOURCES}
   ${QTC_UTILS_SOURCES}
)
include_directories("../3rdparty/")

target_link_libraries(${PROJECT_NAME} Qt5::Core)
target_link_libraries(${PROJECT_NAME} Qt5::Gui)
target_link_libraries(${PROJECT_NAME} Qt5::Widgets)

target_link_libraries(${PROJECT_NAME} ${Exiv2_Library})
include_directories(${Exiv2_IncludePath})
