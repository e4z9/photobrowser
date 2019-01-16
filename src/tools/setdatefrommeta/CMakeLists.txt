cmake_minimum_required(VERSION 3.0.0)
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

project(setdatefrommeta VERSION 0.1 LANGUAGES CXX)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

find_package(Qt5Core)
find_package(Qt5Gui)

add_executable(${PROJECT_NAME}
    "main.cpp"
)
include_directories("../../3rdparty/")

target_link_libraries(${PROJECT_NAME} Qt5::Core)
target_link_libraries(${PROJECT_NAME} Qt5::Gui)

target_sources(${PROJECT_NAME} PRIVATE
    "../../util/fileutil.cpp"
    "../../util/metadatautil.cpp"
)
if(APPLE)
    target_sources(${PROJECT_NAME} PRIVATE
        "../../util/fileutil_mac.mm"
    )
    target_link_libraries(${PROJECT_NAME} "-framework Foundation" "-framework AppKit")
endif(APPLE)

target_link_libraries(${PROJECT_NAME} ${Exiv2_Library})
include_directories(${Exiv2_IncludePath})
