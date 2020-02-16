find_package(gstreamer-1.0 REQUIRED)

add_executable(photobrowser WIN32 MACOSX_BUNDLE
    app.cpp
    app.h
    browserwindow.cpp
    browserwindow.h
    directorytree.cpp
    directorytree.h
    filmrollview.cpp
    filmrollview.h
    fullscreensplitter.cpp
    fullscreensplitter.h
    gstreamer_utils.cpp
    gstreamer_utils.h
    imageview.cpp
    imageview.h
    main.cpp
    mediadirectorymodel.cpp
    mediadirectorymodel.h
    thumbnailcreator.cpp
    thumbnailcreator.h
)

set_target_properties(photobrowser PROPERTIES
    MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/Info.plist.in"
    MACOSX_BUNDLE_GUI_IDENTIFIER "de.zillerey.photobrowser"
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION}
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
)

target_link_libraries(photobrowser sodium-qt thirdparty_qtc util gstreamer)

install(TARGETS photobrowser
    BUNDLE
        DESTINATION .
)
