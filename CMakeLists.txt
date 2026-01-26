## Copyright 2020 The Khronos Group
## SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.17)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_BUILD_TYPE_INIT "Release")

set(CMAKE_INSTALL_RPATH "$ORIGIN")

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

## Top-level project ##
set(USD_DEVICE_VERSION_MAJOR 0)
set(USD_DEVICE_VERSION_MINOR 2)
set(USD_DEVICE_VERSION_PATCH 0)
set(USD_DEVICE_VERSION ${USD_DEVICE_VERSION_MAJOR}.${USD_DEVICE_VERSION_MINOR}.${USD_DEVICE_VERSION_PATCH})
if(NOT USD_DEVICE_BUILD_VERSION_EXPLICIT)
  if(EXISTS ${CMAKE_SOURCE_DIR}/.git)
    find_package(Git REQUIRED)
    execute_process(
      COMMAND ${GIT_EXECUTABLE} rev-list --count HEAD
      WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
      OUTPUT_VARIABLE "USD_DEVICE_BUILD_VERSION"
      ERROR_QUIET
      OUTPUT_STRIP_TRAILING_WHITESPACE
    )
  else()
    set(USD_DEVICE_BUILD_VERSION -1)
  endif()
else()
  set(USD_DEVICE_BUILD_VERSION ${USD_DEVICE_BUILD_VERSION_EXPLICIT})
endif()
message(STATUS "DEVICE VERSION: ${USD_DEVICE_VERSION} build ${USD_DEVICE_BUILD_VERSION}")

project(AnariUsdDevice VERSION ${USD_DEVICE_VERSION} LANGUAGES C CXX)

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
find_package(anari 0.10 REQUIRED
  PATHS ${ANARI_ROOT_DIR}
)

## Code generation

if(CMAKE_VERSION VERSION_GREATER_EQUAL "3.12")
  message(STATUS "Adding codegen target")

  list(PREPEND CMAKE_MODULE_PATH ${Python3_ROOT_DIR})
  find_package(Python3 REQUIRED COMPONENTS Interpreter Development)
  list(POP_FRONT CMAKE_MODULE_PATH)

  add_custom_target(generate_usd_queries
    COMMAND ${Python3_EXECUTABLE} ${ANARI_CODE_GEN_ROOT}/generate_queries.py
      --json ${ANARI_CODE_GEN_ROOT}
      --json ${CMAKE_CURRENT_SOURCE_DIR}
      --prefix UsdDevice
      --device ${CMAKE_CURRENT_SOURCE_DIR}/usd_device.json
      --namespace anari::usd
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    DEPENDS usd_device.json usd_device_features.json khr_geometry_glyph.json
  )

  add_custom_target(generate_all)
  add_dependencies(generate_all generate_usd_queries)
endif()

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
  UsdLibrary.cpp
  UsdSampler.cpp
  UsdMaterial.cpp
  UsdWorld.cpp
  UsdRenderer.cpp
  UsdFrame.cpp
  UsdLight.cpp
  UsdCamera.cpp
  UsdDevice_queries.cpp)

set(USDModule_HEADERS
  UsdCommonMacros.h
  UsdAnari.h
  UsdDeviceUtils.h
  UsdMultiTypeParameter.h
  UsdParameterizedObject.h
  UsdDevice.h
  UsdBaseObject.h
  UsdSharedObjects.h
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
  UsdLight.h
  UsdCamera.h)

add_library(anari_library_usd SHARED ${USDModule_SOURCES} ${USDModule_HEADERS})

target_compile_definitions(anari_library_usd
	PRIVATE
    -DDEVICE_VERSION_BUILD=${USD_DEVICE_BUILD_VERSION}
    -DDEVICE_VERSION_MAJOR=${USD_DEVICE_VERSION_MAJOR}
    -DDEVICE_VERSION_MINOR=${USD_DEVICE_VERSION_MINOR}
    -DDEVICE_VERSION_PATCH=${USD_DEVICE_VERSION_PATCH}
    -DDEVICE_VERSION_NAME="${USD_DEVICE_VERSION}"
    ${USD_DEVICE_TIME_DEFS})

option(CHECK_OBJECT_REFERENCES "Perform a memory check on the reference count of all objects allocated by a USD device, avoiding memleaks and double deletion." OFF)
if(CHECK_OBJECT_REFERENCES)
  target_compile_definitions(anari_library_usd
    PRIVATE -DCHECK_MEMLEAKS)
endif()

target_link_libraries(anari_library_usd
	PUBLIC anari::anari
	PRIVATE anari::helium UsdBridge
)

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
cmake_dependent_option(INSTALL_ANARI_COMPILE_DEPS "Enable install of ANARI SDK dependencies required for compilation of ANARI client code" OFF "INSTALL_ANARI_DEPS" OFF)
if(${INSTALL_ANARI_DEPS})

  set(PLATFORM_INSTALL_LIBDIR "$<$<NOT:$<PLATFORM_ID:Windows>>:${CMAKE_INSTALL_LIBDIR}>$<$<PLATFORM_ID:Windows>:${CMAKE_INSTALL_BINDIR}>/")

  if(${INSTALL_ANARI_COMPILE_DEPS})
    ## Compile time deps
    if(WIN32)
      install(
        DIRECTORY "${ANARI_ROOT_DIR}/bin/"
        DESTINATION ${CMAKE_INSTALL_BINDIR})
    endif()
    install(
      DIRECTORY "${ANARI_ROOT_DIR}/include/"
      DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
    install(
      DIRECTORY "${ANARI_ROOT_DIR}/lib/"
      DESTINATION ${CMAKE_INSTALL_LIBDIR})
  else()
    ## Runtime deps
    install(
      FILES $<TARGET_RUNTIME_DLLS:anari_library_usd>
      DESTINATION ${PLATFORM_INSTALL_LIBDIR})
  endif()
endif()

