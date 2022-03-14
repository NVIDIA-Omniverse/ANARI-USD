message( "-------------------- Begin UsdBridge CmakeLists.txt ----------------------------------------")

if (WIN32)
cmake_minimum_required(VERSION 3.13 FATAL_ERROR)
endif()

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

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
set(USD_INCLUDE_DIR "${USD_DIR}/include")

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

list(APPEND USD_SHARED_LIBS ar arch gf js kind ndr pcp plug sdf tf trace usd usdGeom usdVol usdShade vt work)
message(STATUS "USD_SHARED_LIBS: ${USD_SHARED_LIBS}")

# Python

#if(EXISTS ${USD_PYTHON_ROOT_DIR})
#  if(WIN32)
#    set(PY_LIB_DIR "libs") 
#    set(PY_SHARED_LIB_DIR "${USD_PYTHON_ROOT_DIR}")
#    set(PY_LINK_SUFFIX ${CMAKE_STATIC_LIBRARY_SUFFIX})
#  else()
#    set(PY_LIB_DIR "lib") 
#    set(PY_SHARED_LIB_DIR "${USD_PYTHON_ROOT_DIR}/lib")
#    set(PY_LINK_SUFFIX ${CMAKE_SHARED_LIBRARY_SUFFIX})
#  endif()
#  set(USD_PYTHON_INCLUDE_DIR "${USD_PYTHON_ROOT_DIR}/include")
#  if(NOT EXISTS "${USD_PYTHON_INCLUDE_DIR}/pyconfig.h")
#    file(GLOB USD_PYTHON_INCLUDE_DIR "${USD_PYTHON_ROOT_DIR}/include/python*")
#  endif()
#  message(STATUS "Found USD-Python include dir: ${USD_PYTHON_INCLUDE_DIR}")
#  file(GLOB USD_PYTHON_LINK_LIBS "${USD_PYTHON_ROOT_DIR}/${PY_LIB_DIR}/*${PY_LINK_SUFFIX}")
#  message(STATUS "Found USD-Python libs: ${USD_PYTHON_LINK_LIBS}")
#  set(PYTHON_LINK_DIRS ${USD_LIB_DIR}) 
#else()
  find_package(Python REQUIRED COMPONENTS Development)

  set(USD_PYTHON_LINK_LIBS Python::Module)
  set(PY_SHARED_LIB_DIR "${Python_RUNTIME_LIBRARY_DIRS}")
  set(USD_PYTHON_LINK_DIRS "${Python_LIBRARY_DIRS}") 
#endif()

# Boost may have a custom include directory structure, just take the release ones
file(GLOB Boost_SEP_INCLUDE_DIR "${USD_ROOT_DIR_RELEASE}/include/boost*")
message(STATUS "Boost_SEP_INCLUDE_DIR ${Boost_SEP_INCLUDE_DIR}")

# Bridge source

add_subdirectory(Mdl)
add_subdirectory(Connection)
add_subdirectory(Volume)

set(USDBRIDGE_SOURCES
  UsdBridge.cpp
  Common/UsdBridgeUtils.cpp
  UsdBridgeCaches.cpp
  UsdBridgeUsdWriter.cpp
  UsdBridge.h
  UsdBridgeCaches.h
  UsdBridgeUsdWriter.h
  Common/UsdBridgeData.h
  Common/UsdBridgeUtils.h
  Common/UsdBridgeMacros.h
  usd.h
  ${USDBRIDGE_MDL_SOURCES}
  ${USDBRIDGE_CONNECT_SOURCES}
)

add_library(UsdBridge STATIC ${USDBRIDGE_SOURCES})

# Includes

target_include_directories(UsdBridge
PUBLIC
  ${CMAKE_CURRENT_LIST_DIR}
  ${CMAKE_CURRENT_LIST_DIR}/Common
PRIVATE
  ${USD_INCLUDE_DIR}
  ${CMAKE_CURRENT_LIST_DIR}/Mdl
  ${CMAKE_CURRENT_LIST_DIR}/Connection
  ${CMAKE_CURRENT_LIST_DIR}/Volume
  ${USDBRIDGE_CONNECT_INCLUDE_DIRS}
  ${Boost_SEP_INCLUDE_DIR}
)

target_compile_definitions(UsdBridge PRIVATE
  ${USDBRIDGE_CONNECT_COMPILE_DEFS}
)

# Platform-dependent defs/links

if (WIN32)
  add_definitions("-DBOOST_ALL_DYN_LINK")
  # Combination of boost and python causes pragma linking issues.
  message(STATUS "Python link dirs: ${USD_PYTHON_LINK_DIRS}") 
  target_link_directories(UsdBridge
    PUBLIC "${USD_PYTHON_LINK_DIRS}") 
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
    DIRECTORY "$<$<NOT:$<CONFIG:Debug>>:${USD_ROOT_DIR_RELEASE}/bin/>$<$<CONFIG:Debug>:${USD_ROOT_DIR_DEBUG}/bin/>"
    DESTINATION ${PLATFORM_INSTALL_LIBDIR}
    FILES_MATCHING PATTERN "*${CMAKE_SHARED_LIBRARY_SUFFIX}*")
    
  #Install Python
  if(WIN32)
    file(GLOB PYTHON_RUNTIME_LIBRARIES "${PY_SHARED_LIB_DIR}/*${CMAKE_SHARED_LIBRARY_SUFFIX}*")
  else()
    file(GLOB PYTHON_RUNTIME_LIBRARIES 
      "${PY_SHARED_LIB_DIR}/*libffi.so*"
      "${PY_SHARED_LIB_DIR}/*libpython*.so*")
  endif()

  message(STATUS "Found Python Runtime Libs: ${PYTHON_RUNTIME_LIBRARIES}")
  install(
      FILES ${PYTHON_RUNTIME_LIBRARIES}
      DESTINATION ${PLATFORM_INSTALL_LIBDIR})
endif()

message( "-------------------- End UsdBridge CmakeLists.txt ----------------------------------------")


