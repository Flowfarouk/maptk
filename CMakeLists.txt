cmake_minimum_required(VERSION 2.8.3)
project(MAPTK)

###
# Versioning
#
set(MAPTK_VERSION_MAJOR 0)
set(MAPTK_VERSION_MINOR 5)
set(MAPTK_VERSION_PATCH 0)
set(MAPTK_VERSION "${MAPTK_VERSION_MAJOR}.${MAPTK_VERSION_MINOR}.${MAPTK_VERSION_PATCH}")


###
# Add the CMake directory for CMake modules
#
list(INSERT CMAKE_MODULE_PATH 0
  "${MAPTK_SOURCE_DIR}/CMake"
  )


###
# Options and setup
#
include(CMakeDependentOption)

# we can't build shared libraries on Windows so we leave it off by default
if (WIN32)
  option(BUILD_SHARED_LIBS "Build shared libraries." ON)
else()
  option(BUILD_SHARED_LIBS "Build shared libraries." OFF)
endif()

set(MAPTK_LIB_SUFFIX "" CACHE STRING "String suffix appended to the library directory we install into")


###
# MAPTK CMake config
#
include(maptk-utils)
include(maptk-flags)

set(maptk_export_name maptk_exports)


###
# Dependencies and Modules
#
include( maptk-depends )

# this is where the source code for libraries/plugins lives
add_subdirectory(maptk)

# this is where the executables source code lives
add_subdirectory(tools)


###
# Testing
#

# The following are required to uses Dart and the Cdash dashboard
option(MAPTK_ENABLE_TESTING "Build MAPTK testing" OFF)
if(MAPTK_ENABLE_TESTING)
  enable_testing()
  include(CTest)
  set(BUILD_TESTING ON)
  mark_as_advanced(BUILD_TESTING)
  add_subdirectory(tests)
endif()


###
# Top level installation
#
set(maptk_cmake_install_dir "lib${MAPTK_LIB_SUFFIX}/cmake/maptk")

# Install rules for CMake utilities
include(maptk-install-utils)

# Prepare space-separated list of library names for config
get_property(maptk_libs GLOBAL PROPERTY maptk_libraries)
string(REPLACE ";" " " maptk_libs "${maptk_libs}")

# Configure build-tree CMake config file and export associated targets file
set(MAPTK_CONFIG_FILE "${MAPTK_BINARY_DIR}/maptk-config.cmake")
set(module_path "${MAPTK_SOURCE_DIR}/CMake")
maptk_configure_file(maptk-config
  "${MAPTK_SOURCE_DIR}/CMake/maptk-config.cmake.in"
  "${MAPTK_CONFIG_FILE}"
  MAPTK_SOURCE_DIR MAPTK_BINARY_DIR
  maptk_libs
  module_path
  )
maptk_export_targets("${MAPTK_BINARY_DIR}/maptk-config-targets.cmake")

# Configure install-tree CMake config file and export associated targets file
set(MAPTK_CONFIG_INSTALL_FILE "${MAPTK_BINARY_DIR}/maptk-config-install.cmake")
set(module_path "${CMAKE_INSTALL_PREFIX}/lib${MAPTK_LIB_SUFFIX}/cmake/maptk")
maptk_configure_file(maptk-install-config
  "${MAPTK_SOURCE_DIR}/CMake/maptk-config-install.cmake.in"
  "${MAPTK_CONFIG_INSTALL_FILE}"
  maptk_libs
  module_path
  )
maptk_install(
  FILES       "${MAPTK_CONFIG_INSTALL_FILE}"
  DESTINATION "${maptk_cmake_install_dir}"
  RENAME      maptk-config.cmake
  )
maptk_install(
  EXPORT      ${maptk_export_name}
  DESTINATION "${maptk_cmake_install_dir}"
  FILE        maptk-config-targets.cmake
  )


###
# CPack Packaging
#

#TODO: Define package dependencies
set(MAPTK_DEPS "")

if(EXISTS /etc/redhat-release)
  file(READ /etc/redhat-release RHEL_VERSION)
  string(REGEX REPLACE ".*release ([^\\. ]*).*" "\\1" RHEL_VERSION "${RHEL_VERSION}")
  set(CPACK_SYSTEM_NAME "el${RHEL_VERSION}.${CMAKE_SYSTEM_PROCESSOR}")
  set(CPACK_RPM_PACKAGE_AUTOREQPROV " no")
  set(CPACK_RPM_PACKAGE_REQUIRES "${MAPTK_DEPS}")
else()
  set(CPACK_SYSTEM_NAME "${CMAKE_SYSTEM_NAME}-${CMAKE_SYSTEM_PROCESSOR}")
endif()

set(CPACK_PACKAGE_NAME              "MAPTK")
set(CPACK_PACKAGE_VENDOR            "Kitware, Inc.")
set(CPACK_PACKAGE_CONTACT           "kitware@kitware.com")
set(CPACK_MONOLITHIC_INSTALL        true)
set(CPACK_PACKAGE_VERSION_MAJOR     "${MAPTK_VERSION_MAJOR}")
set(CPACK_PACKAGE_VERSION_MINOR     "${MAPTK_VERSION_MINOR}")
set(CPACK_PACKAGE_VERSION_PATCH     "${MAPTK_VERSION_PATCH}")
set(CPACK_PACKAGE_VERSION           "${MAPTK_VERSION}")
set(CPACK_RESOURCE_FILE_LICENSE     "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
set(CPACK_PACKAGING_INSTALL_PREFIX  "${CMAKE_INSTALL_PREFIX}")
set(CPACK_PACKAGE_FILE_NAME         "${CPACK_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION}-${CPACK_SYSTEM_NAME}")
include (CPack)
