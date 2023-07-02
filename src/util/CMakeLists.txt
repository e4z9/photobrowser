set(CMAKE_AUTOMOC OFF)

add_library(util STATIC
    fileutil.cpp
    fileutil.h
    metadatautil.cpp
    metadatautil.h
    tags.cpp
    tags.h
    util.h
)

target_include_directories(util PUBLIC .. ../3rdparty)
target_link_libraries(util PUBLIC ${Qt}::Core ${Qt}::Gui exiv2lib)

if(APPLE)
    target_sources(util PRIVATE
        fileutil_mac.mm
        util_mac.mm
    )
    target_link_libraries(util PUBLIC ${FWFoundation} ${FWAppKit} ${FWIOKit})
else()
    target_sources(util PRIVATE
        util.cpp
    )
endif(APPLE)

# PlistCpp
find_package(Boost REQUIRED)

target_sources(util
  PRIVATE
    ../3rdparty/PlistCpp/src/Plist.cpp
    ../3rdparty/PlistCpp/src/Plist.hpp
    ../3rdparty/PlistCpp/src/base64.hpp
    ../3rdparty/PlistCpp/src/PlistDate.cpp
    ../3rdparty/PlistCpp/src/PlistDate.hpp
    ../3rdparty/PlistCpp/src/pugiconfig.hpp
    ../3rdparty/PlistCpp/src/pugixml.cpp
    ../3rdparty/PlistCpp/src/pugixml.hpp
)
target_include_directories(util PRIVATE ../3rdparty/PlistCpp/src ${Boost_INCLUDE_DIR})
