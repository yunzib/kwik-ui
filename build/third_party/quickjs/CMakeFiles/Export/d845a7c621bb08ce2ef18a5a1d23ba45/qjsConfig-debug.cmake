#----------------------------------------------------------------
# Generated CMake target import file for configuration "Debug".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "qjs" for configuration "Debug"
set_property(TARGET qjs APPEND PROPERTY IMPORTED_CONFIGURATIONS DEBUG)
set_target_properties(qjs PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_DEBUG "C"
  IMPORTED_LOCATION_DEBUG "${_IMPORT_PREFIX}/lib/libqjs.a"
  )

list(APPEND _cmake_import_check_targets qjs )
list(APPEND _cmake_import_check_files_for_qjs "${_IMPORT_PREFIX}/lib/libqjs.a" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
