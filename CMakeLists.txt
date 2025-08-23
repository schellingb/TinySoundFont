
# config
cmake_minimum_required(VERSION 3.14)
set(PROJECT_VENDOR "schellingb")
set(PROJECT_SNAME "TinySoundFont")
project(TinySoundFont VERSION "0.9")
message(STATUS "version: ${PROJECT_VERSION}")
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

add_library(TinySoundFont INTERFACE)
target_include_directories(
  TinySoundFont INTERFACE
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
  $<INSTALL_INTERFACE:include>)
set_property(
  TARGET TinySoundFont
  APPEND PROPERTY
  PUBLIC_HEADER "tml.h" "tsf.h")
set_target_properties(TinySoundFont PROPERTIES VERSION ${PROJECT_VERSION})

write_basic_package_version_file(
  "${PROJECT_NAME}ConfigVersion.cmake"
  VERSION ${PROJECT_VERSION}
  COMPATIBILITY SameMajorVersion)
  
install(
  TARGETS TinySoundFont
  EXPORT TinySoundFontConfig
  PUBLIC_HEADER
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/tsf"
    COMPONENT devel)
install(
  EXPORT TinySoundFontConfig
    DESTINATION "${CMAKE_INSTALL_DATADIR}/tsf"
    COMPONENT devel)
install(
  FILES "LICENSE" "README.md"
  TYPE DOC
  COMPONENT runtime)
