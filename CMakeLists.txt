## Copyright 2020 The Khronos Group
## SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.17)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_BUILD_TYPE_INIT "Release")

set(CMAKE_INSTALL_RPATH "$ORIGIN")

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

## Top-level project ##

project(AnariUsdDevice LANGUAGES C CXX)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR})
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR})

include(GNUInstallDirs)

add_subdirectory(thirdparty)

## Dependencies ##

if (NOT EXISTS ${ANARI_ROOT_DIR})
  find_path(ANARI_ROOT_DIR NAMES include/anari/anari.h DOC "Path to ANARI SDK")
endif()

set(ANARI_CONFIG_SUBDIR "release")
if(CMAKE_BUILD_TYPE MATCHES "Debug")
  set(ANARI_CONFIG_SUBDIR "debug")  
endif()
if (EXISTS "${ANARI_ROOT_DIR}/${ANARI_CONFIG_SUBDIR}")
  set(ANARI_ROOT_DIR "${ANARI_ROOT_DIR}/${ANARI_CONFIG_SUBDIR}")
endif()

if (NOT EXISTS ${ANARI_ROOT_DIR})
  message(FATAL_ERROR "No valid ANARI_ROOT_DIR set, or found using CMAKE_PREFIX_PATH: ${ANARI_ROOT_DIR}")
else()
  message(STATUS "Using ANARI_ROOT_DIR: ${ANARI_ROOT_DIR}")
endif()

# Search in config mode
find_package(anari 0.2.0 REQUIRED
  PATHS ${ANARI_ROOT_DIR}
)

## Core UsdBridge ##

# Compile defs required by all subdirs
option(USD_DEVICE_TIME_BASED_OUTPUT "Allow use of timesamples for timevarying properties, but no retiming (so all timesteps are global)" ON)
if(USD_DEVICE_TIME_BASED_OUTPUT)
  list(APPEND USD_DEVICE_TIME_DEFS -DTIME_BASED_CACHING)
  option(USD_DEVICE_VALUE_CLIP_OUTPUT "Retiming of timesteps through value clips for selected objects, resulting in separate USD files for those objects" ON)
  if(USD_DEVICE_VALUE_CLIP_OUTPUT)
    list(APPEND USD_DEVICE_TIME_DEFS -DVALUE_CLIP_RETIMING)
    option(USD_DEVICE_CLIP_STAGE_OUTPUT "Separate clip stages for each timestep for selected objects, resulting in a single USD file per timestep" ON)
    if(USD_DEVICE_CLIP_STAGE_OUTPUT)
      list(APPEND USD_DEVICE_TIME_DEFS -DTIME_CLIP_STAGES)
    endif()
  endif()
endif()

add_subdirectory(UsdBridge)

## Build USD Device ##

if(NOT USD_DEVICE_VERSION_EXPLICIT)
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
else()
  set(USD_DEVICE_VERSION ${USD_DEVICE_VERSION_EXPLICIT})
endif()
message(STATUS "USD Device version: ${USD_DEVICE_VERSION}")

set(USDModule_SOURCES
  UsdAnari.cpp
  UsdBaseObject.cpp
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
  UsdFrame.cpp
  UsdLight.cpp)

set(USDModule_HEADERS
  UsdCommonMacros.h
  UsdAnari.h
  UsdDeviceUtils.h
  UsdMultiTypeParameter.h
  UsdParameterizedObject.h
  UsdDevice.h
  UsdBaseObject.h
  UsdBridgedBaseObject.h
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
  UsdFrame.h
  UsdLight.h)

add_library(anari_library_usd SHARED ${USDModule_SOURCES} ${USDModule_HEADERS})

target_compile_definitions(anari_library_usd
	PRIVATE -DDEVICE_VERSION=${USD_DEVICE_VERSION} ${USD_DEVICE_TIME_DEFS})

option(CHECK_OBJECT_REFERENCES "Perform a memory check on the reference count of all objects allocated by a USD device, avoiding memleaks and double deletion." OFF)
if(CHECK_OBJECT_REFERENCES)
  target_compile_definitions(anari_library_usd
    PRIVATE -DCHECK_MEMLEAKS)
endif()

target_link_libraries(anari_library_usd
	PUBLIC anari::anari
	PRIVATE anari::anari_utilities UsdBridge)

option(USD_DEVICE_BUILD_EXAMPLES "Build USD device examples" OFF)
if(USD_DEVICE_BUILD_EXAMPLES)
  add_subdirectory(examples)
endif()

install(TARGETS anari_library_usd
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

## Install optional pdbs
## install(FILES $<TARGET_PDB_FILE:anari_library_usd> DESTINATION ${CMAKE_INSTALL_BINDIR} OPTIONAL)

option(INSTALL_ANARI_DEPS "Enable install of ANARI SDK dependencies along with USD device" OFF) # Not truly a dependency of the USD Device, generally one wants to build against ANARI source, not just libs
if(${INSTALL_ANARI_DEPS})

  set(PLATFORM_INSTALL_LIBDIR "$<$<NOT:$<PLATFORM_ID:Windows>>:${CMAKE_INSTALL_LIBDIR}>$<$<PLATFORM_ID:Windows>:${CMAKE_INSTALL_BINDIR}>/")

  install(
    DIRECTORY "${ANARI_ROOT_DIR}/lib/"
    DESTINATION ${PLATFORM_INSTALL_LIBDIR}
    PATTERN "*${CMAKE_STATIC_LIBRARY_SUFFIX}" EXCLUDE)
  install(
    DIRECTORY "${ANARI_ROOT_DIR}/bin/"
    DESTINATION ${PLATFORM_INSTALL_LIBDIR}
    FILES_MATCHING PATTERN "*${CMAKE_SHARED_LIBRARY_SUFFIX}*")
endif()

