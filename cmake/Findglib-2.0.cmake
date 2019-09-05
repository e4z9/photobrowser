find_library(glib_LIBRARIES NAMES glib-2.0)
find_library(gobject_LIBRARIES NAMES gobject-2.0)
find_path(glib_INCLUDE_DIRS NAMES glib.h PATH_SUFFIXES glib-2.0)
find_path(glibconfig_INCLUDE_DIRS NAMES glibconfig.h PATH_SUFFIXES lib/glib-2.0/include)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(glib DEFAULT_MSG glib_LIBRARIES gobject_LIBRARIES glib_INCLUDE_DIRS glibconfig_INCLUDE_DIRS)

if(glib_FOUND AND NOT TARGET glib)
    if (NOT TARGET gobject)
        add_library(gobject UNKNOWN IMPORTED)
        set_target_properties(gobject PROPERTIES IMPORTED_LOCATION "${gobject_LIBRARIES}")
    endif()
    add_library(glib UNKNOWN IMPORTED)
    set_target_properties(glib PROPERTIES
                          IMPORTED_LOCATION "${glib_LIBRARIES}"
                          INTERFACE_LINK_LIBRARIES gobject
                          INTERFACE_INCLUDE_DIRECTORIES "${glib_INCLUDE_DIRS};${glibconfig_INCLUDE_DIRS}"
    )
endif()

include(FeatureSummary)
set_package_properties(glib PROPERTIES
  URL "https://wiki.gnome.org/Projects/GLib"
  DESCRIPTION "GLib"
)
