## Copyright 2021 NVIDIA Corporation
## SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.16)

if(NOT CMAKE_INSTALL_PREFIX)
  set(CMAKE_INSTALL_PREFIX
      "${CMAKE_BINARY_DIR}/install"
      CACHE STRING "Final install location." FORCE)
endif()

project(anari_usd_superbuild)

include(ExternalProject)
include(ProcessorCount)
include(macros.cmake)

ProcessorCount(PROCESSOR_COUNT)
set(NUM_BUILD_JOBS ${PROCESSOR_COUNT} CACHE STRING "Number of build jobs '-j <n>'")

set(DEFAULT_BUILD_COMMAND cmake --build . --config release -j ${NUM_BUILD_JOBS})

get_filename_component(INSTALL_DIR_ABSOLUTE
  ${CMAKE_INSTALL_PREFIX} ABSOLUTE BASE_DIR ${CMAKE_CURRENT_BINARY_DIR})

## Superbuild options ##

option(BUILD_USD "Build USD and its minimum required dependencies" ON)
option(BUILD_ANARI_SDK "Build ANARI-SDK" ON)
option(BUILD_ANARI_USD_DEVICE "Build ANARI_USD_DEVICE" ON)

## Build projects ##

if (BUILD_USD)
  build_subproject(
    NAME zlib
    URL "http://zlib.net/zlib1211.zip"
    BUILD_ARGS
      -DCMAKE_POSITION_INDEPENDENT_CODE=ON
  )

  build_subproject(
    NAME Blosc
    URL "https://github.com/Blosc/c-blosc/archive/refs/tags/v1.5.4.zip"
    BUILD_ARGS
      -DCMAKE_POSITION_INDEPENDENT_CODE=ON
  )

  build_subproject(
    NAME OpenVDB
    URL "https://github.com/AcademySoftwareFoundation/openvdb/archive/refs/tags/v8.1.0.zip"
    BUILD_ARGS
      -DOPENVDB_BUILD_BINARIES=OFF
      -DCMAKE_POSITION_INDEPENDENT_CODE=ON
    DEPENDS_ON Blosc
  )

  build_subproject(
    NAME USD
    URL "https://github.com/PixarAnimationStudios/USD/archive/refs/tags/v21.08.zip"
    BUILD_ARGS
      -DCMAKE_POSITION_INDEPENDENT_CODE=ON
      -DPXR_BUILD_EXAMPLES=OFF
      -DPXR_BUILD_IMAGING=OFF
      -DPXR_BUILD_TESTS=OFF
      -DPXR_BUILD_TUTORIALS=OFF
      -DPXR_BUILD_USDVIEW=OFF
      -DPXR_BUILD_USD_IMAGING=OFF
      -DPXR_BUILD_USD_TOOLS=OFF
      -DPXR_ENABLE_GL_SUPPORT=OFF
      -DPXR_ENABLE_HDF5_SUPPORT=OFF
      -DPXR_ENABLE_OPENVDB_SUPPORT=ON
      -DPXR_ENABLE_PTEX_SUPPORT=OFF
      -DPXR_USE_PYTHON_3=ON
    DEPENDS_ON OpenVDB zlib
  )
endif()

if (BUILD_ANARI_SDK)
  build_subproject(
    NAME ANARI-SDK
    URL "https://github.com/KhronosGroup/ANARI-SDK/archive/refs/heads/main.zip"
    BUILD_ARGS
      -DBUILD_EXAMPLES=OFF
      -DBUILD_TESTS=OFF
      -DCMAKE_POSITION_INDEPENDENT_CODE=ON
  )
endif()

if (BUILD_ANARI_USD_DEVICE)
  if (BUILD_USD)
    list(APPEND DEPS USD)
  endif()
  if (BUILD_ANARI_SDK)
    list(APPEND DEPS ANARI-SDK)
  endif()
  build_subproject(
    NAME AnariUsdDevice
    SOURCE_DIR ${CMAKE_CURRENT_LIST_DIR}/..
    BUILD_ARGS
      -DCMAKE_POSITION_INDEPENDENT_CODE=ON
    DEPENDS_ON ${DEPS}
  )
endif()