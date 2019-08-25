cmake_minimum_required(VERSION 3.0.0)
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

project(photobrowser VERSION 0.1 LANGUAGES CXX)

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(SodiumCxx REQUIRED)
find_package(Qt5 COMPONENTS Core Gui Widgets Multimedia MultimediaWidgets REQUIRED)

set(QTC_UTILS_SOURCES
    "../3rdparty/qtc/progressindicator.cpp"
    "../3rdparty/qtc/runextensions.cpp"
    "../3rdparty/qtc/utils.qrc"
)
add_compile_definitions(QTCREATOR_UTILS_EXPORT=)

add_executable(${PROJECT_NAME} WIN32 MACOSX_BUNDLE
   "app.cpp"
   "browserwindow.cpp"
   "directorytree.cpp"
   "filmrollview.cpp"
   "fullscreensplitter.cpp"
   "imageview.cpp"
   "main.cpp"
   "mediadirectorymodel.cpp"
   "thumbnailcreator.cpp"
   ${QTC_UTILS_SOURCES}
)
include_directories("../3rdparty/")

set_target_properties(${PROJECT_NAME} PROPERTIES
    MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/Info.plist.in"
    MACOSX_BUNDLE_GUI_IDENTIFIER "de.zillerey.photobrowser"
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION}
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
)

target_link_libraries(${PROJECT_NAME} Qt5::Core Qt5::Gui Qt5::Widgets
    Qt5::Multimedia Qt5::MultimediaWidgets SodiumCxx)

target_sources(${PROJECT_NAME} PRIVATE
    "../util/fileutil.cpp"
    "../util/metadatautil.cpp"
)
if(APPLE)
    target_sources(${PROJECT_NAME} PRIVATE
        "../util/fileutil_mac.mm"
    )
    target_link_libraries(${PROJECT_NAME} "-framework Foundation" "-framework AppKit")
endif(APPLE)

target_link_libraries(${PROJECT_NAME} ${Exiv2_Library})
include_directories(${Exiv2_IncludePath})
