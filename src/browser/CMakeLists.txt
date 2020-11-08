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

    INSTALL_RPATH "@executable_path/../Frameworks"
)

target_link_libraries(photobrowser sodium-qt thirdparty_qtc util gstreamer-1.0)

install(TARGETS photobrowser
    BUNDLE
        DESTINATION .
    COMPONENT deploy
)

get_target_property(_moc_binary Qt5::moc IMPORTED_LOCATION)
get_filename_component(_qt_bin_path "${_moc_binary}" DIRECTORY)
get_filename_component(_qt_path "${_qt_bin_path}/../" ABSOLUTE)

install(
    CODE "execute_process(COMMAND stack \"${PROJECT_SOURCE_DIR}/scripts/deploy.hs\" -t \"\${CMAKE_INSTALL_PREFIX}/photobrowser.app\" -q \"${_qt_path}\")"
    COMPONENT deploy
    EXCLUDE_FROM_ALL
)
