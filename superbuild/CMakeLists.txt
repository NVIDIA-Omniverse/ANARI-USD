## Copyright 2021 NVIDIA Corporation
## SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.16)

if(NOT CMAKE_INSTALL_PREFIX)
  if(NOT CMAKE_BUILD_TYPE)
    set(INSTALL_DIR_EXPRESSION "${CMAKE_BINARY_DIR}/install/$<CONFIG>")
  else()
    set(INSTALL_DIR_EXPRESSION "${CMAKE_BINARY_DIR}/install")
  endif()

  set(CMAKE_INSTALL_PREFIX
      "${INSTALL_DIR_EXPRESSION}"
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

option(BUILD_USD "Build USD and its minimum required dependencies" OFF)
option(BUILD_ANARI_SDK "Build ANARI-SDK" ON)
option(BUILD_ANARI_USD_DEVICE "Build ANARI_USD_DEVICE" ON)

option(USD_DEVICE_USE_OPENVDB "Enable OpenVDB support for USD device" OFF)
option(USD_DEVICE_USE_OMNIVERSE "Enable Omniverse support for USD device" OFF)

option(USD_DEVICE_INSTALL_DEPS "Install USD (and Omniverse) dependencies along with the device" OFF)

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
  if(EXISTS "${TBB_DIR}/../../../include")
    file(COPY "${TBB_DIR}/../../../" DESTINATION ${CMAKE_INSTALL_PREFIX})
  else()
    message(FATAL_ERROR "'TBB_DIR/../../../' does not contain include and lib directory")
  endif()

  #build_subproject(
  #  NAME boost
  #  URL "https://downloads.sourceforge.net/project/boost/boost/1.70.0/boost_1_70_0.tar.gz"
  #  BUILD_ARGS
  #    -DCMAKE_POSITION_INDEPENDENT_CODE=ON
  #)

  find_package(boost REQUIRED) # Rely on pre-installed boost for now
  if(EXISTS "${boost_DIR}/../../../include")
    file(COPY "${boost_DIR}/../../../" DESTINATION "${CMAKE_INSTALL_PREFIX}/")
  else()
    message(FATAL_ERROR "'boost_DIR/../../../' does not contain include and lib directory")
  endif()

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

  set(USD_ROOT_DIR "" CACHE PATH "Path to USD")
  set(Python_ROOT_DIR "" CACHE PATH "Path to python install used for building USD")

  if(NOT EXISTS ${USD_ROOT_DIR})
    message(FATAL_ERROR "When BUILD_USD is off, a valid directory is required for USD_ROOT_DIR (optionally pointing to debug/ and release/ subdirectories)")
  endif()

  if(NOT WIN32 AND USD_DEVICE_USE_OPENVDB)
    set(ZLIB_ROOT "" CACHE PATH "Path to ZLIB install used for building USD")
    if(NOT EXISTS ${ZLIB_ROOT})
      message(FATAL_ERROR "On Linux, a valid directory is required for ZLIB_ROOT (the ZLIB install used for building USD)")
    endif()
  endif()

endif()

if(USD_DEVICE_USE_OPENVDB)
  option(USE_USD_OPENVDB_BUILD "If USD source installation exists, use OpenVDB from there" ON)

  if(NOT USE_USD_OPENVDB_BUILD)
    set(OPENVDB_ROOT_DIR "" CACHE PATH "Path of OpenVDB install with static library")

    if(NOT EXISTS ${OPENVDB_ROOT_DIR})
      message(FATAL_ERROR "With USD_DEVICE_USE_OPENVDB and not USE_USD_OPENVDB_BUILD, a valid directory is required for OPENVDB_ROOT_DIR (optionally pointing to debug/ and release/ subdirectories)")
    endif()
  endif()
endif()

if(USD_DEVICE_USE_OMNIVERSE)
  set(OMNICLIENT_ROOT_DIR "" CACHE PATH "Path to Omniverse Client")

  if(NOT EXISTS ${OMNICLIENT_ROOT_DIR})
    message(FATAL_ERROR "When USD_DEVICE_USE_OMNIVERSE is on, a valid directory is required for OMNICLIENT_ROOT_DIR (optionally pointing to debug/ and release/ subdirectories)")
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
else()
  set(ANARI_ROOT_DIR "" CACHE PATH "Path to ANARI SDK")

  if(NOT EXISTS ${ANARI_ROOT_DIR})
    message(FATAL_ERROR "When BUILD_ANARI_SDK is off, a valid directory is required for ANARI_ROOT_DIR")
  endif()
endif()

if (BUILD_ANARI_USD_DEVICE)
  set(ANARI_DEVICE_BUILD_ARGS
    -DCMAKE_POSITION_INDEPENDENT_CODE=ON
    -DUSD_DEVICE_USE_OPENVDB=${USD_DEVICE_USE_OPENVDB}
    -DUSD_DEVICE_USE_OMNIVERSE=${USD_DEVICE_USE_OMNIVERSE}
    -DUSE_USD_OPENVDB_BUILD=${USE_USD_OPENVDB_BUILD}
    -DINSTALL_USD_DEPS=${USD_DEVICE_INSTALL_DEPS}
    -DINSTALL_OMNIVERSE_DEPS=${USD_DEVICE_INSTALL_DEPS}
    -DINSTALL_ANARI_DEPS=$<NOT:$<BOOL:${BUILD_ANARI_SDK}>>)

  if(USD_DEVICE_VERSION_EXPLICIT)
    list(APPEND ANARI_DEVICE_BUILD_ARGS
      -DUSD_DEVICE_VERSION_EXPLICIT=${USD_DEVICE_VERSION_EXPLICIT})
  endif()

  if (BUILD_USD)
    list(APPEND DEPS USD)
  else()
    list(APPEND ANARI_DEVICE_BUILD_ARGS
      -DUSD_ROOT_DIR=${USD_ROOT_DIR}
      -DPython_ROOT_DIR=${Python_ROOT_DIR}
      -DPython_FIND_STRATEGY=LOCATION
      )
    if(USD_DEVICE_USE_OPENVDB)
      list(APPEND ANARI_DEVICE_BUILD_ARGS -DOPENVDB_ROOT_DIR=${OPENVDB_ROOT_DIR})
      if(NOT WIN32)
        list(APPEND ANARI_DEVICE_BUILD_ARGS -DZLIB_ROOT=${ZLIB_ROOT})
      endif()
    endif()
    if(USD_DEVICE_USE_OMNIVERSE)
      list(APPEND ANARI_DEVICE_BUILD_ARGS -DOMNICLIENT_ROOT_DIR=${OMNICLIENT_ROOT_DIR})
      if(NOT WIN32) # Assume Omniverse-enabled builds use the nvidia-based USD build, which requires CXX11_ABI=0
        list(APPEND ANARI_DEVICE_BUILD_ARGS -DCMAKE_CXX_FLAGS="-D_GLIBCXX_USE_CXX11_ABI=0")
      endif()
    endif()
  endif()

  if (BUILD_ANARI_SDK)
    list(APPEND DEPS ANARI-SDK)
  else()
    list(APPEND ANARI_DEVICE_BUILD_ARGS
      -DANARI_ROOT_DIR=${ANARI_ROOT_DIR})
  endif()

  build_subproject(
    NAME AnariUsdDevice
    SOURCE_DIR ${CMAKE_CURRENT_LIST_DIR}/..
    BUILD_ARGS ${ANARI_DEVICE_BUILD_ARGS}
    DEPENDS_ON ${DEPS}
  )
endif()
