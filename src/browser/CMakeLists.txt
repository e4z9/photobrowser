find_package(Qt6 REQUIRED COMPONENTS Multimedia LinguistTools)

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
    imageview.cpp
    imageview.h
    main.cpp
    mediadirectorymodel.cpp
    mediadirectorymodel.h
    tagsview.cpp
    tagsview.h
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

target_link_libraries(photobrowser PRIVATE sodium-qt thirdparty_qtc util
  Qt::Multimedia Qt::Concurrent)

qt_standard_project_setup(I18N_TRANSLATED_LANGUAGES de)
qt_add_translations(photobrowser
    NO_GENERATE_PLURALS_TS_FILE
    TS_FILE_BASE photobrowser
    TS_OUTPUT_DIRECTORY translations
)

install(TARGETS photobrowser
    BUNDLE
        DESTINATION .
    COMPONENT deploy
)
