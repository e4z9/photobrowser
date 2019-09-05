find_package(glib-2.0 REQUIRED)

find_library(gstreamer_LIBRARIES NAMES gstreamer-1.0)
find_path(gstreamer_INCLUDE_DIRS NAMES gst/gst.h PATH_SUFFIXES gstreamer-1.0)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(gstreamer DEFAULT_MSG gstreamer_LIBRARIES gstreamer_INCLUDE_DIRS)

if(gstreamer_FOUND AND NOT TARGET gstreamer)
  add_library(gstreamer UNKNOWN IMPORTED)
  set_target_properties(gstreamer PROPERTIES
                        IMPORTED_LOCATION "${gstreamer_LIBRARIES}"
                        INTERFACE_LINK_LIBRARIES glib
                        INTERFACE_INCLUDE_DIRECTORIES "${gstreamer_INCLUDE_DIRS}"
  )
endif()

include(FeatureSummary)
set_package_properties(gstreamer PROPERTIES
  URL "https://gstreamer.freedesktop.org/"
  DESCRIPTION "Open source multimedia framework"
)
