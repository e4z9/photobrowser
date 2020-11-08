find_package(glib-2.0 REQUIRED)

find_library(gstreamer-1.0_LIBRARIES NAMES gstreamer-1.0)
find_path(gstreamer-1.0_INCLUDE_DIRS NAMES gst/gst.h PATH_SUFFIXES gstreamer-1.0)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(gstreamer-1.0 DEFAULT_MSG gstreamer-1.0_LIBRARIES gstreamer-1.0_INCLUDE_DIRS)

if(gstreamer-1.0_FOUND AND NOT TARGET gstreamer-1.0)
  add_library(gstreamer-1.0 UNKNOWN IMPORTED)
  set_target_properties(gstreamer-1.0 PROPERTIES
                        IMPORTED_LOCATION "${gstreamer-1.0_LIBRARIES}"
                        INTERFACE_LINK_LIBRARIES glib-2.0
                        INTERFACE_INCLUDE_DIRECTORIES "${gstreamer-1.0_INCLUDE_DIRS}"
  )
endif()

include(FeatureSummary)
set_package_properties(gstreamer-1.0 PROPERTIES
  URL "https://gstreamer.freedesktop.org/"
  DESCRIPTION "Open source multimedia framework"
)
