find_package(Boost REQUIRED) # boost should be a public dependency of sodium-cxx instead

if(TARGET libsodium)
  # from submodule
  add_library(SodiumCxx ALIAS libsodium)
  set_target_properties(libsodium PROPERTIES
                        INTERFACE_INCLUDE_DIRECTORIES "${sodium_BINARY_DIR}/include;${Boost_INCLUDE_DIR}")
else()
  find_library(SodiumCxx_LIBRARIES NAMES libsodium.a) # hmpf
  find_path(SodiumCxx_INCLUDE_DIRS NAMES sodium/sodium.h)

  include(FindPackageHandleStandardArgs)
  find_package_handle_standard_args(SodiumCxx DEFAULT_MSG SodiumCxx_LIBRARIES SodiumCxx_INCLUDE_DIRS)

  if(SodiumCxx_FOUND AND NOT TARGET SodiumCxx)
    add_library(SodiumCxx UNKNOWN IMPORTED)
    set_target_properties(SodiumCxx PROPERTIES
                          IMPORTED_LOCATION "${SodiumCxx_LIBRARIES}"
                          INTERFACE_INCLUDE_DIRECTORIES "${SodiumCxx_INCLUDE_DIRS};${Boost_INCLUDE_DIR}"
    )
  endif()
endif()

include(FeatureSummary)
set_package_properties(SodiumCxx PROPERTIES
  URL "https://github.com/SodiumFRP/sodium-cxx"
  DESCRIPTION "Sodium Functional Reactive Programming (FRP) Framework for C++"
)
