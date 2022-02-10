message( "-------------------- Begin UsdBridge CmakeLists.txt ----------------------------------------")

if (WIN32)
cmake_minimum_required(VERSION 3.13 FATAL_ERROR)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# USD

if (NOT EXISTS ${USD_INSTALL_DIR})
  find_path(USD_INSTALL_DIR NAMES include/pxr/pxr.h DOC "Path to USD")
  message(STATUS "Using USD_INSTALL_DIR: ${USD_INSTALL_DIR}")
endif()

if (NOT EXISTS ${USD_INSTALL_DIR})
  message(FATAL_ERROR "No valid USD_INSTALL_DIR set, or found using CMAKE_PREFIX_PATH: ${USD_INSTALL_DIR}")
endif()

message(STATUS "Using cmake value of USD_INSTALL_DIR: ${USD_INSTALL_DIR}")
if(EXISTS "${USD_INSTALL_DIR}/release")
  set(USD_INSTALL_DIR_RELEASE "${USD_INSTALL_DIR}/release")
else()
  set(USD_INSTALL_DIR_RELEASE "${USD_INSTALL_DIR}")
endif()
if(EXISTS "${USD_INSTALL_DIR}/debug")
  set(USD_INSTALL_DIR_DEBUG "${USD_INSTALL_DIR}/debug")
else()
  set(USD_INSTALL_DIR_DEBUG "${USD_INSTALL_DIR}")
endif()

if(CMAKE_BUILD_TYPE MATCHES "Debug")
  if(NOT EXISTS "${USD_INSTALL_DIR_DEBUG}/include")
    message(FATAL_ERROR "USD_INSTALL_DIR or its /debug subfolder does not have an /include subfolder, so it's not configured correctly at ${USD_INSTALL_DIR_DEBUG}")
  endif()
elseif(NOT EXISTS "${USD_INSTALL_DIR_RELEASE}/include")
  message(FATAL_ERROR "USD_INSTALL_DIR or its /release subfolder does not have an /include subfolder, so it's not configured correctly at ${USD_INSTALL_DIR_RELEASE}")
endif()

set(USD_DIR "$<$<NOT:$<CONFIG:Debug>>:${USD_INSTALL_DIR_RELEASE}>$<$<CONFIG:Debug>:${USD_INSTALL_DIR_DEBUG}>")
set(USD_INCLUDE_DIR "${USD_DIR}/include")

if(CMAKE_CONFIGURATION_TYPES)
  set(USD_LIB_DIR "${USD_DIR}/lib")
else()
  # Generator-expression-free, due to find_library later on
  if(CMAKE_BUILD_TYPE MATCHES "Debug")
    set(USD_LIB_DIR "${USD_INSTALL_DIR_DEBUG}/lib")
  else()
    set(USD_LIB_DIR "${USD_INSTALL_DIR_RELEASE}/lib")
  endif()
endif()

list(APPEND USD_SHARED_LIBS ar arch gf js kind ndr pcp plug sdf tf trace usd usdGeom usdVol usdShade vt work)
message(STATUS "USD_SHARED_LIBS: ${USD_SHARED_LIBS}")

# Python

set(USD_PYTHON_INSTALL_DIR "" CACHE PATH "Path of python version corresponding to USD dependency")

set(PREV_PYTHON_ROOT_DIR ${Python_ROOT_DIR})

if(EXISTS ${USD_PYTHON_INSTALL_DIR})
  if(WIN32)
    set(PY_LIB_DIR "libs") 
    set(PY_LINK_SUFFIX ${CMAKE_STATIC_LIBRARY_SUFFIX})
  else()
    set(PY_LIB_DIR "lib") 
    set(PY_LINK_SUFFIX ${CMAKE_SHARED_LIBRARY_SUFFIX})
  endif()
  set(USD_PYTHON_INCLUDE_DIR "${USD_PYTHON_INSTALL_DIR}/include")
  if(NOT EXISTS "${USD_PYTHON_INCLUDE_DIR}/pyconfig.h")
    file(GLOB USD_PYTHON_INCLUDE_DIR "${USD_PYTHON_INSTALL_DIR}/include/python*")
  endif()
  message(STATUS "Found USD-Python include dir: ${USD_PYTHON_INCLUDE_DIR}")
  file(GLOB USD_PYTHON_LINK_LIBS "${USD_PYTHON_INSTALL_DIR}/${PY_LIB_DIR}/*${PY_LINK_SUFFIX}")
  message(STATUS "Found USD-Python libs: ${USD_PYTHON_LINK_LIBS}")
else()
  find_package(Python REQUIRED COMPONENTS Development)
  set(USD_PYTHON_INCLUDE_DIR ${Python_INCLUDE_DIRS})
  set(USD_PYTHON_LINK_LIBS Python::Module)
endif()

set(Python_ROOT_DIR ${PREV_PYTHON_ROOT_DIR})

# Bridge source

add_subdirectory(Mdl)
add_subdirectory(Connection)
add_subdirectory(Volume)

set(USDBRIDGE_SOURCES
  UsdBridge.cpp
  UsdBridgeUtils.cpp
  UsdBridgeCaches.cpp
  UsdBridgeUsdWriter.cpp
  UsdBridge.h
  UsdBridgeCaches.h
  UsdBridgeUsdWriter.h
  UsdBridgeData.h
  UsdBridgeUtils.h
  UsdBridgeMacros.h
  usd.h
  ${USDBRIDGE_MDL_SOURCES}
  ${USDBRIDGE_CONNECT_SOURCES}
)

add_library(UsdBridge STATIC ${USDBRIDGE_SOURCES})

# Includes

target_include_directories(UsdBridge
PUBLIC
  ${CMAKE_CURRENT_LIST_DIR}
PRIVATE
  ${USD_INCLUDE_DIR}
  ${CMAKE_CURRENT_LIST_DIR}/Mdl
  ${CMAKE_CURRENT_LIST_DIR}/Connection
  ${CMAKE_CURRENT_LIST_DIR}/Volume
  ${USD_PYTHON_INCLUDE_DIR}
  ${USDBRIDGE_CONNECT_INCLUDE_DIRS}
)

target_compile_definitions(UsdBridge PRIVATE
  ${USDBRIDGE_CONNECT_COMPILE_DEFS}
)

# Platform-dependent defs/links

if (WIN32)
  add_definitions("-DBOOST_ALL_DYN_LINK")
  set_source_files_properties(${CMAKE_CURRENT_LIST_DIR}/Volume/UsdBridgeVolumeWriter.cpp PROPERTIES COMPILE_FLAGS /bigobj)
else()
  set(PLATFORM_LIBS pthread dl stdc++fs)
  target_link_libraries(UsdBridge PRIVATE general ${PLATFORM_LIBS} )

  # Assume compilation against the Nvidia Omniverse USD libraries,
  # which are not built with the CXX11 ABI
  if(${USD_DEVICE_USE_OMNIVERSE})
    target_compile_definitions(UsdBridge PRIVATE
      _GLIBCXX_USE_CXX11_ABI=0 )
  endif()

  #target_link_options(UsdBridge PUBLIC
  #  "-Wl,--rpath-link,${USD_LIB_DIR}"
  #  ${USDBRIDGE_CONNECT_LINK_OPTIONS})  # Not required for ANARI
endif()

# Linking

if(CMAKE_CONFIGURATION_TYPES)
  # Due to USD_LIB_DIR being a generator expression, we can't search for absolute dirs. Instead keep libs relative
  target_link_directories(UsdBridge PUBLIC
    ${USD_LIB_DIR}
    ${USDBRIDGE_CONNECT_LINK_DIRS}
  )
  foreach(lib ${USD_SHARED_LIBS})
    list(APPEND USD_LIBS ${lib})
  endforeach()
else()
  foreach(lib ${USD_SHARED_LIBS})
    find_library(path ${lib} PATHS ${USD_LIB_DIR} NO_DEFAULT_PATH)
    list(APPEND USD_LIBS ${path})
    unset(path CACHE)
  endforeach()
endif()

target_link_libraries(UsdBridge 
  PRIVATE
    UsdBridge_Volume
    ${USD_LIBS}
    ${USD_PYTHON_LINK_LIBS}
    ${USDBRIDGE_CONNECT_LINK_LIBS}
)

#Install UsdBridge

install(TARGETS UsdBridge
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

option(INSTALL_USD_DEPS "Enable install of USD dependencies with USD device" ON)
if(${INSTALL_USD_DEPS})

  set(PLATFORM_INSTALL_LIBDIR "$<$<NOT:$<PLATFORM_ID:Windows>>:${CMAKE_INSTALL_LIBDIR}>$<$<PLATFORM_ID:Windows>:${CMAKE_INSTALL_BINDIR}>/")

  #Install USD
  install(
    DIRECTORY "${USD_LIB_DIR}/" 
    DESTINATION ${PLATFORM_INSTALL_LIBDIR}
    PATTERN "*${CMAKE_STATIC_LIBRARY_SUFFIX}" EXCLUDE)
  install(
    DIRECTORY "$<$<NOT:$<CONFIG:Debug>>:${USD_INSTALL_DIR_RELEASE}/bin/>$<$<CONFIG:Debug>:${USD_INSTALL_DIR_DEBUG}/bin/>"
    DESTINATION ${PLATFORM_INSTALL_LIBDIR}
    FILES_MATCHING PATTERN "*${CMAKE_SHARED_LIBRARY_SUFFIX}*")
    
  #Install Python
  if(EXISTS ${USD_PYTHON_INSTALL_DIR})
    file(GLOB PYTHON_RUNTIME_LIBRARIES "${USD_PYTHON_INSTALL_DIR}/*${CMAKE_SHARED_LIBRARY_SUFFIX}*")
  else()
    file(GLOB PYTHON_RUNTIME_LIBRARIES "${Python_RUNTIME_LIBRARY_DIRS}/*${CMAKE_SHARED_LIBRARY_SUFFIX}*")
  endif()
  install(
      FILES ${PYTHON_RUNTIME_LIBRARIES}
      DESTINATION ${PLATFORM_INSTALL_LIBDIR})
endif()

message( "-------------------- End UsdBridge CmakeLists.txt ----------------------------------------")


