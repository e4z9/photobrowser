set(CMAKE_AUTOMOC OFF)

add_library(sodium-qt STATIC
    sqaction.cpp
    sqaction.h
    sqcheckbox.cpp
    sqcheckbox.h
    sqeditbase.h
    sqlineedit.cpp
    sqlineedit.h
    sqlistview.cpp
    sqlistview.h
    sqtimer.cpp
    sqtimer.h
    sqtools.cpp
    sqtools.h
)

target_include_directories(sodium-qt PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(sodium-qt PUBLIC Qt5::Widgets SodiumCxx)

