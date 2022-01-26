## Copyright 2020 The Khronos Group
## SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.17)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_BUILD_TYPE_INIT "Release")

set(CMAKE_INSTALL_RPATH "$ORIGIN")

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set(PLATFORM_INSTALL_LIBDIR "$<$<NOT:$<PLATFORM_ID:Windows>>:${CMAKE_INSTALL_LIBDIR}>$<$<PLATFORM_ID:Windows>:${CMAKE_INSTALL_BINDIR}>/")

## Top-level project ##

project(AnariUsdDevice LANGUAGES C CXX)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_PROJECT_DIR})
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_PROJECT_DIR})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_PROJECT_DIR})

include(GNUInstallDirs)

## Dependencies ##

if (NOT EXISTS ${ANARI_INSTALL_DIR})
  find_path(ANARI_INSTALL_DIR NAMES include/anari/anari.h DOC "Path to ANARI SDK")
  message(STATUS "Using ANARI_INSTALL_DIR: ${ANARI_INSTALL_DIR}")
endif()

if (NOT EXISTS ${ANARI_INSTALL_DIR})
  message(FATAL_ERROR "No valid ANARI_INSTALL_DIR set, or found using CMAKE_PREFIX_PATH: ${ANARI_INSTALL_DIR}")
endif()

if (EXISTS "${ANARI_INSTALL_DIR}/release")
  # Use the release install for now, until release/debug are in the same package
  set(ANARI_INSTALL_DIR "${ANARI_INSTALL_DIR}/release")
endif()

list(APPEND CMAKE_PREFIX_PATH ${ANARI_INSTALL_DIR})
find_package(anari REQUIRED)

## Core UsdBride ##

add_subdirectory(UsdBridge)

## Build USD Device ##

if(EXISTS ${CMAKE_SOURCE_DIR}/.git AND IS_DIRECTORY ${CMAKE_SOURCE_DIR}/.git)
  find_package(Git REQUIRED)
  execute_process(
    COMMAND ${GIT_EXECUTABLE} rev-list --count HEAD
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    OUTPUT_VARIABLE "USD_DEVICE_VERSION"
    ERROR_QUIET
    OUTPUT_STRIP_TRAILING_WHITESPACE
  )
else()
  set(USD_DEVICE_VERSION -1)
endif()

set(USDModule_SOURCES
  UsdAnari.cpp
  UsdDevice.cpp
  UsdDataArray.cpp
  UsdGeometry.cpp
  UsdSurface.cpp
  UsdGroup.cpp
  UsdSpatialField.cpp
  UsdVolume.cpp
  UsdInstance.cpp
  UsdSampler.cpp
  UsdMaterial.cpp
  UsdWorld.cpp
  UsdRenderer.cpp
  UsdFrame.cpp)

set(USDModule_HEADERS
  UsdCommonMacros.h
  UsdAnari.h
  UsdParameterizedObject.h
  UsdDevice.h
  UsdBaseObject.h
  UsdDataArray.h
  UsdGeometry.h
  UsdSurface.h
  UsdGroup.h
  UsdSpatialField.h
  UsdVolume.h
  UsdInstance.h
  UsdSampler.h
  UsdMaterial.h
  UsdWorld.h
  UsdRenderer.h
  UsdFrame.h)


add_library(anari_library_usd SHARED ${USDModule_SOURCES} ${USDModule_HEADERS})

target_compile_definitions(anari_library_usd
	PRIVATE -DDEVICE_VERSION=${USD_DEVICE_VERSION})

target_link_libraries(anari_library_usd
	PUBLIC anari::anari
	PRIVATE anari::anari_utilities UsdBridge)

add_subdirectory(examples)

install(TARGETS anari_library_usd
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

option(INSTALL_ANARI_DEPS "Enable install of ANARI SDK dependencies along with USD device" ON)
if(${INSTALL_ANARI_DEPS})
  install(
    DIRECTORY "${ANARI_INSTALL_DIR}/lib/" 
    DESTINATION ${PLATFORM_INSTALL_LIBDIR}
    PATTERN "*${CMAKE_STATIC_LIBRARY_SUFFIX}" EXCLUDE)
  install(
    DIRECTORY "${ANARI_INSTALL_DIR}/bin/" 
    DESTINATION ${PLATFORM_INSTALL_LIBDIR}
    FILES_MATCHING PATTERN "*${CMAKE_SHARED_LIBRARY_SUFFIX}")
endif()

