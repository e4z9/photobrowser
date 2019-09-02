set(CMAKE_AUTOMOC OFF)

add_library(util STATIC
    fileutil.cpp
    fileutil.h
    metadatautil.cpp
    metadatautil.h
)

target_include_directories(util PUBLIC .. ../3rdparty)
target_link_libraries(util PUBLIC Qt5::Core Qt5::Gui Exiv2)

if(APPLE)
    target_sources(util PRIVATE
        fileutil_mac.mm
    )
    target_link_libraries(util PUBLIC ${FWFoundation} ${FWAppKit})
endif(APPLE)
