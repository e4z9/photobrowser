add_library(thirdparty_qtc STATIC
    functiontraits.h
    progressindicator.cpp
    progressindicator.h
    runextensions.cpp
    runextensions.h
    utils.qrc
)

target_include_directories(thirdparty_qtc PUBLIC ..)

target_link_libraries(thirdparty_qtc PUBLIC ${Qt}::Widgets)
target_compile_definitions(thirdparty_qtc PUBLIC QTCREATOR_UTILS_EXPORT=)
