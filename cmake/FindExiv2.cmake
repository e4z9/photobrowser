find_library(Exiv2_LIBRARIES NAMES exiv2)
find_path(Exiv2_INCLUDE_DIRS NAMES exiv2/exiv2.hpp)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Exiv2 DEFAULT_MSG Exiv2_LIBRARIES Exiv2_INCLUDE_DIRS)

if(Exiv2_FOUND AND NOT TARGET Exiv2)
  add_library(Exiv2 UNKNOWN IMPORTED)
  set_target_properties(Exiv2 PROPERTIES
                        IMPORTED_LOCATION "${Exiv2_LIBRARIES}"
                        INTERFACE_INCLUDE_DIRECTORIES "${Exiv2_INCLUDE_DIRS}"
  )
endif()

include(FeatureSummary)
set_package_properties(Exiv2 PROPERTIES
  URL "https://www.exiv2.org"
  DESCRIPTION "C++ metadata library and tools"
)
