message( "-------------------- Begin UsdBridge CmakeLists.txt ----------------------------------------")

if (WIN32)
cmake_minimum_required(VERSION 3.15 FATAL_ERROR)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(CMakeDependentOption)
include(usdhelpers.cmake)

# USD

if (NOT EXISTS ${USD_ROOT_DIR})
  find_path(USD_ROOT_DIR NAMES include/pxr/pxr.h DOC "Path to USD")
  message(STATUS "Using USD_ROOT_DIR: ${USD_ROOT_DIR}")
endif()

if (NOT EXISTS ${USD_ROOT_DIR})
  message(FATAL_ERROR "No valid USD_ROOT_DIR set, or found using CMAKE_PREFIX_PATH: ${USD_ROOT_DIR}")
endif()

message(STATUS "Using cmake value of USD_ROOT_DIR: ${USD_ROOT_DIR}")
if(EXISTS "${USD_ROOT_DIR}/release")
  set(USD_ROOT_DIR_RELEASE "${USD_ROOT_DIR}/release")
else()
  set(USD_ROOT_DIR_RELEASE "${USD_ROOT_DIR}")
endif()
if(EXISTS "${USD_ROOT_DIR}/debug")
  set(USD_ROOT_DIR_DEBUG "${USD_ROOT_DIR}/debug")
else()
  set(USD_ROOT_DIR_DEBUG "${USD_ROOT_DIR}")
endif()

if(CMAKE_BUILD_TYPE MATCHES "Debug")
  if(NOT EXISTS "${USD_ROOT_DIR_DEBUG}/include")
    message(FATAL_ERROR "USD_ROOT_DIR or its /debug subfolder does not have an /include subfolder, so it's not configured correctly at ${USD_ROOT_DIR_DEBUG}")
  endif()
elseif(NOT EXISTS "${USD_ROOT_DIR_RELEASE}/include")
  message(FATAL_ERROR "USD_ROOT_DIR or its /release subfolder does not have an /include subfolder, so it's not configured correctly at ${USD_ROOT_DIR_RELEASE}")
endif()

set(USD_DIR "$<$<NOT:$<CONFIG:Debug>>:${USD_ROOT_DIR_RELEASE}>$<$<CONFIG:Debug>:${USD_ROOT_DIR_DEBUG}>")
if(CMAKE_CONFIGURATION_TYPES)
  set(USD_LIB_DIR "${USD_DIR}/lib")
else()
  # Generator-expression-free, due to find_library later on
  if(CMAKE_BUILD_TYPE MATCHES "Debug")
    set(USD_LIB_DIR "${USD_ROOT_DIR_DEBUG}/lib")
  else()
    set(USD_LIB_DIR "${USD_ROOT_DIR_RELEASE}/lib")
  endif()
endif()

# Search in config mode
find_package(pxr REQUIRED
  PATHS ${USD_ROOT_DIR_RELEASE})

if(NOT (USD_ROOT_DIR_DEBUG STREQUAL USD_ROOT_DIR_RELEASE))
  set(USD_DEBUG_TARGET_CMAKE "${USD_ROOT_DIR_DEBUG}/cmake/pxrTargets-debug.cmake")
  if(EXISTS ${USD_DEBUG_TARGET_CMAKE})
    set(_IMPORT_PREFIX ${USD_ROOT_DIR_DEBUG})
    #include USD debug cmake targets
    include(${USD_DEBUG_TARGET_CMAKE})
    set(_IMPORT_PREFIX)
  endif()
endif()

list(APPEND USD_INIT_TARGETS usdGeom usdVol usdShade usdLux usdUtils)
flatten_and_cleanup_targets(USD_TARGETS "${USD_INIT_TARGETS}")
message(STATUS "USD_TARGETS: ${USD_TARGETS}")

# Python

list(PREPEND CMAKE_MODULE_PATH ${Python_ROOT_DIR})
find_package(Python REQUIRED COMPONENTS Development)
list(POP_FRONT CMAKE_MODULE_PATH)

set(USD_PYTHON_TARGETS Python::Module)
set(PY_SHARED_LIB_DIR "${Python_RUNTIME_LIBRARY_DIRS}")
set(USD_PYTHON_LINK_DIRS "${Python_LIBRARY_DIRS}")

# Boost may have a custom include directory structure, just take the release ones

file(GLOB Boost_SEP_INCLUDE_DIR "${USD_ROOT_DIR_RELEASE}/include/boost*")
message(STATUS "Boost_SEP_INCLUDE_DIR ${Boost_SEP_INCLUDE_DIR}")

# Bridge source

add_subdirectory(Common)
add_subdirectory(Mdl)
add_subdirectory(Connection)
add_subdirectory(Volume)

set(USDBRIDGE_SOURCES
  UsdBridge.cpp
  UsdBridgeCaches.cpp
  UsdBridgeUsdWriter.cpp
  UsdBridgeUsdWriter_Geometry.cpp
  UsdBridgeUsdWriter_Material.cpp
  UsdBridgeUsdWriter_Volume.cpp
  UsdBridgeTimeEvaluator.cpp
  UsdBridgeRt.cpp
  UsdBridgeDiagnosticMgrDelegate.cpp
  UsdBridge.h
  UsdBridgeCaches.h
  UsdBridgeUsdWriter.h
  UsdBridgeUsdWriter_Common.h
  UsdBridgeUsdWriter_Arrays.h
  UsdBridgeTimeEvaluator.h
  UsdBridgeRt.h
  UsdBridgeDiagnosticMgrDelegate.h
  usd.h
  ${USDBRIDGE_MDL_SOURCES}
)

add_library(UsdBridge STATIC ${USDBRIDGE_SOURCES})

if (WIN32)
  set_source_files_properties(
    ${CMAKE_CURRENT_LIST_DIR}/UsdBridgeRt.cpp
    ${CMAKE_CURRENT_LIST_DIR}/UsdBridgeUsdWriter_Geometry.cpp
    PROPERTIES COMPILE_FLAGS /bigobj)
endif()

# Includes

target_include_directories(UsdBridge
  PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}
  PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/Mdl
    ${Boost_SEP_INCLUDE_DIR}
  )

# Defs

target_compile_definitions(UsdBridge
	PRIVATE ${USD_DEVICE_TIME_DEFS})

# Platform-dependent operations

if (WIN32)
  target_compile_definitions(UsdBridge PRIVATE "-DBOOST_ALL_DYN_LINK")
  # Figure out the vc toolset of the boost libs included with the USD install
  file(GLOB Boost_libfiles "${USD_ROOT_DIR_RELEASE}/lib/boost*")
  if(Boost_libfiles)
    list(GET Boost_libfiles 0 Boost_libfile)
    get_filename_component(Boost_libfile_name ${Boost_libfile} NAME)
    string(REGEX MATCH "vc[0-9]*" Boost_lib_toolset ${Boost_libfile_name})
    message(STATUS "Usd-boost VC toolset: ${Boost_lib_toolset}")
    target_compile_definitions(UsdBridge PRIVATE "-DBOOST_LIB_TOOLSET=\"${Boost_lib_toolset}\"")
  endif()
  # Combination of boost and python causes pragma linking issues for both libraries.
  message(STATUS "Python link dirs: ${USD_PYTHON_LINK_DIRS}")
  # Make sure to set link dirs public, since this is a static lib, so the dynamic boost libs will propagate to the parent project. 
  target_link_directories(UsdBridge PUBLIC "${USD_PYTHON_LINK_DIRS}" "${USD_LIB_DIR}")
else()
  set(PLATFORM_LIBS pthread dl stdc++fs)
  target_compile_definitions(UsdBridge PRIVATE _GLIBCXX_USE_CXX11_ABI=0 )
  target_link_libraries(UsdBridge PRIVATE general ${PLATFORM_LIBS} )
endif()

# Linking

target_link_libraries(UsdBridge
  PUBLIC  
    UsdBridge_Common
  PRIVATE
    stb_image
    UsdBridge_Volume
    UsdBridge_Connect
    ${USD_TARGETS}
    ${USD_PYTHON_TARGETS}
)

# USDRT/CarbSDK

option(USD_DEVICE_USE_USDRT "Enable usage of USDRT by supplying <USDRT/CARBSDK>_<INCLUDE/BIN>_DIR and CUDAToolkit_ROOT" OFF)
cmake_dependent_option(USD_DEVICE_USE_FABRIC "Enable usage of Fabric" OFF "USD_DEVICE_USE_USDRT" OFF)
if(${USD_DEVICE_USE_USDRT})
  find_package(CUDAToolkit)
  target_include_directories(UsdBridge
    PRIVATE
      ${USDRT_INCLUDE_DIR}
      "${USDRT_INCLUDE_DIR}/usdrt_only"
      "${USDRT_INCLUDE_DIR}/../../gsl/include"
      "${USDRT_INCLUDE_DIR}/../../pybind11/include"
      "${USDRT_INCLUDE_DIR}/../../cuda/include"
      "${USDRT_INCLUDE_DIR}/../.."
      ${CARBSDK_INCLUDE_DIR}
  )
  target_link_libraries(UsdBridge PRIVATE "${CARBSDK_BIN_DIR}/carb${CMAKE_STATIC_LIBRARY_SUFFIX}" CUDA::cudart)

  target_compile_definitions(UsdBridge PRIVATE USE_USDRT)
  if(${USD_DEVICE_USE_FABRIC})
    target_compile_definitions(UsdBridge PRIVATE USE_FABRIC)
  endif()
endif()

#Install UsdBridge

#install(TARGETS UsdBridge
#  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
#  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
#  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
#)

option(INSTALL_USD_DEPS "Enable install of USD dependencies with USD device" OFF)
if(${INSTALL_USD_DEPS})

  set(PLATFORM_INSTALL_LIBDIR "$<$<NOT:$<PLATFORM_ID:Windows>>:${CMAKE_INSTALL_LIBDIR}>$<$<PLATFORM_ID:Windows>:${CMAKE_INSTALL_BINDIR}>/")

  #Install USD
  install(
    DIRECTORY "${USD_LIB_DIR}/"
    DESTINATION ${PLATFORM_INSTALL_LIBDIR}
    PATTERN "*${CMAKE_STATIC_LIBRARY_SUFFIX}" EXCLUDE)
  install(
    DIRECTORY "$<$<NOT:$<CONFIG:Debug>>:${USD_ROOT_DIR_RELEASE}/bin/>$<$<CONFIG:Debug>:${USD_ROOT_DIR_DEBUG}/bin/>"
    DESTINATION ${PLATFORM_INSTALL_LIBDIR}
    FILES_MATCHING PATTERN "*${CMAKE_SHARED_LIBRARY_SUFFIX}*")

  #Install Python
  message(STATUS "Python runtime library dirs: ${PY_SHARED_LIB_DIR}")
  if(WIN32)
    file(GLOB PYTHON_RUNTIME_LIBRARIES "${PY_SHARED_LIB_DIR}/python*${CMAKE_SHARED_LIBRARY_SUFFIX}*")
  else()
    file(GLOB PYTHON_RUNTIME_LIBRARIES
      "${PY_SHARED_LIB_DIR}/*libffi.so*"
      "${PY_SHARED_LIB_DIR}/*libpython*.so*")
  endif()
  message(STATUS "Found Python Runtime Libs: ${PYTHON_RUNTIME_LIBRARIES}")
  install(
      FILES ${PYTHON_RUNTIME_LIBRARIES}
      DESTINATION ${PLATFORM_INSTALL_LIBDIR})

  #Install CarbSdk
  if(${USD_DEVICE_USE_USDRT})
  install(
    DIRECTORY "${USDRT_BIN_DIR}/"
    DESTINATION ${PLATFORM_INSTALL_LIBDIR}
    FILES_MATCHING
    PATTERN "*${CMAKE_SHARED_LIBRARY_SUFFIX}*"
    PATTERN "*.pdb"
    REGEX "${USDRT_BIN_DIR}(/[^/]*)+/[^/]*$") # Any file that's not in the root of the bin dir will match
  install(
    DIRECTORY "${CARBSDK_BIN_DIR}/"
    DESTINATION ${PLATFORM_INSTALL_LIBDIR}
    FILES_MATCHING
    PATTERN "*${CMAKE_SHARED_LIBRARY_SUFFIX}*"
    PATTERN "*.pdb"
    REGEX "${CARBSDK_BIN_DIR}(/[^/]*)+/[^/]*$")
  install(
    DIRECTORY "${RTXPLUGINS_BIN_DIR}/"
    DESTINATION ${PLATFORM_INSTALL_LIBDIR}
    FILES_MATCHING
    REGEX "${RTXPLUGINS_BIN_DIR}/plugins/omni.usd/[^/]*$")
  install(
    DIRECTORY "${RTXPLUGINS_BIN_DIR}/"
    DESTINATION ${PLATFORM_INSTALL_LIBDIR}
    FILES_MATCHING
    REGEX "${RTXPLUGINS_BIN_DIR}/plugins/omni.usd/[^/]*$")
  install(
    DIRECTORY "${CUDAToolkit_ROOT}/../${PLATFORM_INSTALL_LIBDIR}"
    DESTINATION ${PLATFORM_INSTALL_LIBDIR}
    FILES_MATCHING
    REGEX "/cudart[0-9_]*${CMAKE_SHARED_LIBRARY_SUFFIX}$")
endif()
endif()

message( "-------------------- End UsdBridge CmakeLists.txt ----------------------------------------")


