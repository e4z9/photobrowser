find_library(glib-2.0_LIBRARIES NAMES glib-2.0)
find_library(gobject_LIBRARIES NAMES gobject-2.0)
find_path(glib-2.0_INCLUDE_DIRS NAMES glib.h PATH_SUFFIXES glib-2.0)
find_path(glibconfig-2.0_INCLUDE_DIRS NAMES glibconfig.h PATH_SUFFIXES lib/glib-2.0/include)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(glib-2.0 DEFAULT_MSG glib-2.0_LIBRARIES gobject_LIBRARIES glib-2.0_INCLUDE_DIRS glibconfig-2.0_INCLUDE_DIRS)

if(glib-2.0_FOUND AND NOT TARGET glib-2.0)
    if (NOT TARGET gobject)
        add_library(gobject UNKNOWN IMPORTED)
        set_target_properties(gobject PROPERTIES IMPORTED_LOCATION "${gobject_LIBRARIES}")
    endif()
    add_library(glib-2.0 UNKNOWN IMPORTED)
    set_target_properties(glib-2.0 PROPERTIES
                          IMPORTED_LOCATION "${glib-2.0_LIBRARIES}"
                          INTERFACE_LINK_LIBRARIES gobject
                          INTERFACE_INCLUDE_DIRECTORIES "${glib-2.0_INCLUDE_DIRS};${glibconfig-2.0_INCLUDE_DIRS}"
    )
endif()

include(FeatureSummary)
set_package_properties(glib-2.0 PROPERTIES
  URL "https://wiki.gnome.org/Projects/GLib"
  DESCRIPTION "GLib"
)
