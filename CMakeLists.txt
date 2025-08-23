
# config
cmake_minimum_required(VERSION 3.14)
set(PROJECT_VENDOR "schellingb")
set(PROJECT_SNAME "TinySoundFont")
project(TinySoundFont VERSION "0.9")
message(STATUS "version: ${PROJECT_VERSION}")
include(GNUInstallDirs)

add_library(TinySoundFont INTERFACE)
target_include_directories(
  TinySoundFont INTERFACE
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
  $<INSTALL_INTERFACE:include>)
set_property(
  TARGET TinySoundFont
  APPEND PROPERTY
  PUBLIC_HEADER "tml.h" "tsf.h")
set_target_properties(TinySoundFont PROPERTIES OUTPUT_NAME tinysoundfont)
set_target_properties(TinySoundFont PROPERTIES VERSION ${PROJECT_VERSION})

install(
  TARGETS TinySoundFont
  EXPORT TinySoundFontTargets
  PUBLIC_HEADER
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/tinysoundfont"
    COMPONENT devel)
install(
  EXPORT TinySoundFontTargets
    DESTINATION "${CMAKE_INSTALL_DATADIR}/tinysoundfont"
    COMPONENT devel)
install(
  FILES "LICENSE" "README.md"
  TYPE DOC
  COMPONENT runtime)
