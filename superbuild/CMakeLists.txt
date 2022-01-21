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

if(WIN32)
  set(DEFAULT_CONFIG_STRING "$(Configuration)")
else()
  set(DEFAULT_CONFIG_STRING "release")
endif()
set(DEFAULT_BUILD_COMMAND cmake --build . --config ${DEFAULT_CONFIG_STRING} -j ${NUM_BUILD_JOBS})

get_filename_component(INSTALL_DIR_ABSOLUTE
  ${CMAKE_INSTALL_PREFIX} ABSOLUTE BASE_DIR ${CMAKE_CURRENT_BINARY_DIR})

## Superbuild options ##

option(BUILD_USD "Build USD and its minimum required dependencies" ON)
option(BUILD_ANARI_SDK "Build ANARI-SDK" ON)
option(BUILD_ANARI_USD_DEVICE "Build ANARI_USD_DEVICE" ON)

option(USD_DEVICE_USE_OPENVDB "Enable OpenVDB support for USD device" OFF)
option(USD_DEVICE_USE_OMNIVERSE "Enable Omniverse support for USD device" OFF)

## Build projects ##

if (BUILD_USD)
  
  find_package(Python3 REQUIRED Interpreter Development)

  #if(WIN32)
  #  set(TBB_URL "https://github.com/oneapi-src/oneTBB/releases/download/2018_U6/tbb2018_20180822oss_win.zip")
  #else()
  #  set(TBB_URL "https://github.com/oneapi-src/oneTBB/archive/2018_U6.tar.gz")
  #endif()

  #build_subproject(
  #  NAME tbb
  #  URL ${TBB_URL}
  #  BUILD_ARGS
  #    -DCMAKE_POSITION_INDEPENDENT_CODE=ON
  #)
  
  find_package(TBB REQUIRED) # Rely on pre-installed tbb for now
  
  #build_subproject(
  #  NAME boost
  #  URL "https://downloads.sourceforge.net/project/boost/boost/1.70.0/boost_1_70_0.tar.gz"
  #  BUILD_ARGS
  #    -DCMAKE_POSITION_INDEPENDENT_CODE=ON
  #)
  
  find_package(boost REQUIRED) # Rely on pre-installed boost for now
  
  build_subproject(
    NAME zlib
    URL "http://zlib.net/zlib1211.zip"
    BUILD_ARGS
      -DCMAKE_POSITION_INDEPENDENT_CODE=ON
  )
  
  build_subproject(
    NAME ilmbase
    URL "https://github.com/AcademySoftwareFoundation/openexr/archive/v2.3.0.zip"
    BUILD_ARGS
      -DCMAKE_POSITION_INDEPENDENT_CODE=ON
      -DPYTHON_LIBRARY=${Python3_LIBRARY_DIRS}
      -DPYTHON_INCLUDE_DIR=${Python3_INCLUDE_DIRS}
      -DOPENEXR_PYTHON_MAJOR=${Python3_VERSION_MAJOR}
      -DOPENEXR_PYTHON_MINOR=${Python3_VERSION_MINOR}
    DEPENDS_ON
      zlib
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
    DEPENDS_ON 
      #tbb 
      #boost 
      ilmbase 
      Blosc
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
      -DPXR_ENABLE_OPENVDB_SUPPORT=${USD_DEVICE_USE_OPENVDB}
      -DPXR_ENABLE_PTEX_SUPPORT=OFF
      -DPXR_USE_PYTHON_3=ON
    DEPENDS_ON OpenVDB zlib
  )

  if(USD_DEVICE_USE_OMNIVERSE)
    message(FATAL_ERROR "Cannot use Omniverse binaries in combination with regular USD binaries, turn off BUILD_USD and use Omniverse-specific USD instead")
  endif()

else()

  set(USD_INSTALL_DIR "" CACHE PATH "Path to USD")
  set(USD_PYTHON_INSTALL_DIR "" CACHE PATH "Path of python version corresponding to USD dependency")  
  
  if(NOT EXISTS ${USD_INSTALL_DIR})
    message(FATAL_ERROR "When BUILD_USD is off, a valid directory is required for USD_INSTALL_DIR (optionally pointing to debug/ and release/ subdirectories)")
  endif()
  
endif()

if(USD_DEVICE_USE_OPENVDB)
  option(USE_USD_OPENVDB_BUILD "If USD source installation exists, use OpenVDB from there" ON)
  
  if(NOT USE_USD_OPENVDB_BUILD)
    set(OPENVDB_INSTALL_DIR "" CACHE PATH "Path of OpenVDB install with static library")
    
    if(NOT EXISTS ${OPENVDB_INSTALL_DIR})
      message(FATAL_ERROR "With USD_DEVICE_USE_OPENVDB and not USE_USD_OPENVDB_BUILD, a valid directory is required for OPENVDB_INSTALL_DIR (optionally pointing to debug/ and release/ subdirectories)")
    endif()
  endif()
endif()

if(USD_DEVICE_USE_OMNIVERSE)
  set(OMNI_CLIENT_INSTALL_DIR "" CACHE PATH "Path to Omniverse Client")
  
  if(NOT EXISTS ${OMNI_CLIENT_INSTALL_DIR})
    message(FATAL_ERROR "When USD_DEVICE_USE_OMNIVERSE is on, a valid directory is required for OMNI_CLIENT_INSTALL_DIR (optionally pointing to debug/ and release/ subdirectories)")
  endif()
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
  set(ANARI_DEVICE_BUILD_ARGS
    -DCMAKE_POSITION_INDEPENDENT_CODE=ON
    -DUSD_DEVICE_USE_OPENVDB=${USD_DEVICE_USE_OPENVDB}
    -DUSD_DEVICE_USE_OMNIVERSE=${USD_DEVICE_USE_OMNIVERSE}
    -DUSE_USD_OPENVDB_BUILD=${USE_USD_OPENVDB_BUILD})
  
  if (BUILD_USD)
    list(APPEND DEPS USD)
  else()
    list(APPEND ANARI_DEVICE_BUILD_ARGS
      -DUSD_INSTALL_DIR=${USD_INSTALL_DIR}
      -DUSD_PYTHON_INSTALL_DIR=${USD_PYTHON_INSTALL_DIR}
      -DOPENVDB_INSTALL_DIR=${OPENVDB_INSTALL_DIR}
      -DOMNI_CLIENT_INSTALL_DIR=${OMNI_CLIENT_INSTALL_DIR})
  endif()
  
  if (BUILD_ANARI_SDK)
    list(APPEND DEPS ANARI-SDK)
  endif()

  build_subproject(
    NAME AnariUsdDevice
    SOURCE_DIR ${CMAKE_CURRENT_LIST_DIR}/..
    BUILD_ARGS ${ANARI_DEVICE_BUILD_ARGS}
    DEPENDS_ON ${DEPS}
  )
endif()
