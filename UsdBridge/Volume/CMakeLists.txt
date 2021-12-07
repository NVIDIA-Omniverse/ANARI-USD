message( "-------------------- Begin UsdBridgeVolume CmakeLists.txt ----------------------------------------")

option(USD_DEVICE_USE_OPENVDB "Enable OpenVDB support for USD device" OFF)

###########
# OpenVDB #
###########

# OpenVDB #

if(${USD_DEVICE_USE_OPENVDB})

  file(GLOB USD_OPENVDB_BUILD "${USD_INSTALL_DIR_RELEASE}/include/openvdb")
  option(USE_USD_OPENVDB_BUILD "If USD source installation exists, use OpenVDB from there" ON)
  
  set(OPENVDB_INSTALL_DIR "" CACHE PATH "Path of OpenVDB install with static library")
  if(${USE_USD_OPENVDB_BUILD} AND EXISTS ${USD_OPENVDB_BUILD})
    set(OPENVDB_INSTALL_DIR ${USD_INSTALL_DIR} CACHE PATH "" FORCE)
  endif()

  MESSAGE(STATUS "OpenVDB enabled, using cmake value of OPENVDB_INSTALL_DIR: ${OPENVDB_INSTALL_DIR}")

  if(EXISTS "${OPENVDB_INSTALL_DIR}/release")
    set(OPENVDB_INSTALL_DIR_RELEASE "${OPENVDB_INSTALL_DIR}/release")
  else()
    set(OPENVDB_INSTALL_DIR_RELEASE "${OPENVDB_INSTALL_DIR}")
  endif()
  if(EXISTS "${OPENVDB_INSTALL_DIR}/debug")
    set(OPENVDB_INSTALL_DIR_DEBUG "${OPENVDB_INSTALL_DIR}/debug")
  else()
    set(OPENVDB_INSTALL_DIR_DEBUG "${OPENVDB_INSTALL_DIR}")
  endif()
  
   file(GLOB OPENVDB_OPENEXR_BUILD "${OPENVDB_INSTALL_DIR_RELEASE}/include/OpenEXR") # Also allow OpenEXR/blosc/zlib to exist within the OpenVDB directory instead

  if(${USE_USD_OPENVDB_BUILD} AND ((EXISTS ${USD_OPENVDB_BUILD}) OR (EXISTS ${OPENVDB_OPENEXR_BUILD})))
    set(OPENVDB_FROM_USD_INSTALL 1)
  else()
    set(OPENVDB_FROM_USD_INSTALL 0)
  endif()
  
  if(${OPENVDB_FROM_USD_INSTALL})
    # OPENVDB_INSTALL_DIR remains the same, and will include OpenEXR, blosc, zlib if USE_USD_OPENVDB_BUILD
    MESSAGE(STATUS "Using OpenVDB from USD install")
  else()
    # No possibility of finding an openvdb version originating from a usd install 
    set(USE_USD_OPENVDB_BUILD OFF CACHE BOOL "" FORCE) 
    MESSAGE(STATUS "NOT Using OpenVDB from USD install (USE_USD_OPENVDB_BUILD not set, or <USD_INSTALL_DIR>/[<config>]/include/openvdb missing, or <OPENVDB_INSTALL_DIR>/[<config>]/include/OpenEXR missing")
  endif()
                                                                                                                                                                  
  if(CMAKE_BUILD_TYPE MATCHES "Debug")
    if(NOT EXISTS "${OPENVDB_INSTALL_DIR_DEBUG}/include")
      message(FATAL_ERROR "OPENVDB_INSTALL_DIR or its /debug subfolder does not have an /include subfolder, so it's not configured correctly at ${OPENVDB_INSTALL_DIR_DEBUG}")
    endif()
  elseif(NOT EXISTS "${OPENVDB_INSTALL_DIR_RELEASE}/include")
    message(FATAL_ERROR "OPENVDB_INSTALL_DIR or its /release subfolder does not have an /include subfolder, so it's not configured correctly at ${OPENVDB_INSTALL_DIR_RELEASE}")
  endif()
  
  set(OPENVDB_DIR "$<$<NOT:$<CONFIG:Debug>>:${OPENVDB_INSTALL_DIR_RELEASE}>$<$<CONFIG:Debug>:${OPENVDB_INSTALL_DIR_DEBUG}>")
  set(OPENVDB_INCLUDE_DIR "${OPENVDB_DIR}/include")
  
  unset(OPENVDB_LIB_FILE_RELEASE CACHE)
  unset(OPENVDB_LIB_FILE_DEBUG CACHE)
  if(${OPENVDB_FROM_USD_INSTALL}) #Retain just the else clause upon USD-OpenVDB build arg fix
    if (WIN32)
      set(OPENVDB_FILE_NAME libopenvdb)
    else()
      set(OPENVDB_FILE_NAME libopenvdb.so)
    endif()
  else()
    if (WIN32)
      set(OPENVDB_FILE_NAME libopenvdb)
    else()
      set(OPENVDB_FILE_NAME libopenvdb.a)
    endif()
  endif()
  find_library(OPENVDB_LIB_FILE_RELEASE ${OPENVDB_FILE_NAME} PATHS "${OPENVDB_INSTALL_DIR_RELEASE}/lib" REQUIRED)
  find_library(OPENVDB_LIB_FILE_DEBUG ${OPENVDB_FILE_NAME} PATHS "${OPENVDB_INSTALL_DIR_DEBUG}/lib" REQUIRED)
  message(STATUS "OpenVDB lib files: ${OPENVDB_LIB_FILE_RELEASE}, ${OPENVDB_LIB_FILE_DEBUG}")
  set(OPENVDB_LIB_FILE "$<$<NOT:$<CONFIG:Debug>>:${OPENVDB_LIB_FILE_RELEASE}>$<$<CONFIG:Debug>:${OPENVDB_LIB_FILE_DEBUG}>")
  
  # OpenEXR #
  
  set(OPENEXR_INSTALL_DIR "" CACHE PATH "Path of OpenEXR install with static library")
  if(${USE_USD_OPENVDB_BUILD} AND EXISTS ${USD_OPENVDB_BUILD})
    set(OPENEXR_INSTALL_DIR ${USD_INSTALL_DIR} CACHE PATH "" FORCE)
  elseif(${USE_USD_OPENVDB_BUILD} AND EXISTS ${OPENVDB_OPENEXR_BUILD})
    set(OPENEXR_INSTALL_DIR ${OPENVDB_INSTALL_DIR} CACHE PATH "" FORCE)                                                                      
  endif()
  mark_as_advanced(OPENEXR_INSTALL_DIR)
  
  if(EXISTS "${OPENEXR_INSTALL_DIR}/release")
    set(OPENEXR_INSTALL_DIR_RELEASE "${OPENEXR_INSTALL_DIR}/release")
  else()
    set(OPENEXR_INSTALL_DIR_RELEASE "${OPENEXR_INSTALL_DIR}")
  endif()
  if(EXISTS "${OPENEXR_INSTALL_DIR}/debug")
    set(OPENEXR_INSTALL_DIR_DEBUG "${OPENEXR_INSTALL_DIR}/debug")
  else()
    set(OPENEXR_INSTALL_DIR_DEBUG "${OPENEXR_INSTALL_DIR}")
  endif()
  
  set(OPENEXR_INCLUDE_DIR "${OPENEXR_INSTALL_DIR}/include")
  
  unset(ILMBASE_HALF_LIB_FILE_RELEASE CACHE)
  unset(ILMBASE_HALF_LIB_FILE_DEBUG CACHE)
  if(${OPENVDB_FROM_USD_INSTALL})
    find_library(ILMBASE_HALF_LIB_FILE_RELEASE Half PATHS "${OPENEXR_INSTALL_DIR_RELEASE}/lib")
    find_library(ILMBASE_HALF_LIB_FILE_DEBUG Half PATHS "${OPENEXR_INSTALL_DIR_DEBUG}/lib")
    if(NOT EXISTS ${ILMBASE_HALF_LIB_FILE_RELEASE})
      if (WIN32)
        file(GLOB ILMBASE_HALF_LIB_FILE_RELEASE "${OPENEXR_INSTALL_DIR_RELEASE}/lib/Half-?_?.lib" REQUIRED)
        file(GLOB ILMBASE_HALF_LIB_FILE_DEBUG "${OPENEXR_INSTALL_DIR_DEBUG}/lib/Half-?_?_d.lib" REQUIRED)
      else()
        file(GLOB ILMBASE_HALF_LIB_FILE_RELEASE "${OPENEXR_INSTALL_DIR_RELEASE}/lib/libHalf-?_?.*" REQUIRED)
        file(GLOB ILMBASE_HALF_LIB_FILE_DEBUG "${OPENEXR_INSTALL_DIR_DEBUG}/lib/libHalf-?_?_d.*" REQUIRED)
      endif()    
    endif()
  else()
    if (WIN32)
      file(GLOB ILMBASE_HALF_LIB_FILE_RELEASE "${OPENEXR_INSTALL_DIR_RELEASE}/lib/Half-?_?_s.lib")
      file(GLOB ILMBASE_HALF_LIB_FILE_DEBUG "${OPENEXR_INSTALL_DIR_DEBUG}/lib/Half-?_?_s_d.lib")
    else()
      file(GLOB ILMBASE_HALF_LIB_FILE_RELEASE "${OPENEXR_INSTALL_DIR_RELEASE}/lib/libHalf-?_?_static.*")
      file(GLOB ILMBASE_HALF_LIB_FILE_DEBUG "${OPENEXR_INSTALL_DIR_DEBUG}/lib/libHalf-?_?_static_d.*")
    endif()  
    if(NOT EXISTS ${ILMBASE_HALF_LIB_FILE_RELEASE})
      file(GLOB ILMBASE_HALF_LIB_FILE_RELEASE "${OPENEXR_INSTALL_DIR_RELEASE}/lib/libHalf-?_?.a" REQUIRED)
      file(GLOB ILMBASE_HALF_LIB_FILE_DEBUG "${OPENEXR_INSTALL_DIR_DEBUG}/lib/libHalf-?_?.a" REQUIRED)
    endif()
  endif()
  message(STATUS "IlmBase lib files: ${ILMBASE_HALF_LIB_FILE_RELEASE}, ${ILMBASE_HALF_LIB_FILE_DEBUG}")
  set(ILMBASE_HALF_LIB_FILE "$<$<NOT:$<CONFIG:Debug>>:${ILMBASE_HALF_LIB_FILE_RELEASE}>$<$<CONFIG:Debug>:${ILMBASE_HALF_LIB_FILE_DEBUG}>")
  
  # Blosc #
  
  set(BLOSC_INSTALL_DIR "" CACHE PATH "Path of Blosc install with static library")
  if(${USE_USD_OPENVDB_BUILD} AND EXISTS ${USD_OPENVDB_BUILD})
    set(BLOSC_INSTALL_DIR ${USD_INSTALL_DIR} CACHE PATH "" FORCE)
  elseif(${USE_USD_OPENVDB_BUILD} AND EXISTS ${OPENVDB_OPENEXR_BUILD})
    set(BLOSC_INSTALL_DIR ${OPENVDB_INSTALL_DIR} CACHE PATH "" FORCE)                                                                      
  endif()
  mark_as_advanced(BLOSC_INSTALL_DIR)
  
  if(EXISTS "${BLOSC_INSTALL_DIR}/release")
    set(BLOSC_INSTALL_DIR_RELEASE "${BLOSC_INSTALL_DIR}/release")
  else()
    set(BLOSC_INSTALL_DIR_RELEASE "${BLOSC_INSTALL_DIR}")
  endif()
  if(EXISTS "${BLOSC_INSTALL_DIR}/debug")
    set(BLOSC_INSTALL_DIR_DEBUG "${BLOSC_INSTALL_DIR}/debug")
  else()
    set(BLOSC_INSTALL_DIR_DEBUG "${BLOSC_INSTALL_DIR}")
  endif()
  
  unset(BLOSC_LIB_FILE_RELEASE CACHE)
  unset(BLOSC_LIB_FILE_DEBUG CACHE)
  if (WIN32)
    set(BLOSC_FILE_NAME libblosc)
  else()
    set(BLOSC_FILE_NAME libblosc.a)
  endif()
  find_library(BLOSC_LIB_FILE_RELEASE ${BLOSC_FILE_NAME} PATHS "${BLOSC_INSTALL_DIR_RELEASE}/lib" REQUIRED)
  find_library(BLOSC_LIB_FILE_DEBUG ${BLOSC_FILE_NAME} PATHS "${BLOSC_INSTALL_DIR_DEBUG}/lib" REQUIRED)
  message(STATUS "Blosc lib files: ${BLOSC_LIB_FILE_RELEASE}, ${BLOSC_LIB_FILE_DEBUG}")
  set(BLOSC_LIB_FILE "$<$<NOT:$<CONFIG:Debug>>:${BLOSC_LIB_FILE_RELEASE}>$<$<CONFIG:Debug>:${BLOSC_LIB_FILE_DEBUG}>")
  
  # ZLib #
  
  set(ZLIB_INSTALL_DIR "" CACHE PATH "Path of Zlib install with static library")
  if(WIN32 AND ${USE_USD_OPENVDB_BUILD} AND EXISTS ${USD_OPENVDB_BUILD})
    set(ZLIB_INSTALL_DIR ${USD_INSTALL_DIR} CACHE PATH "" FORCE)
  elseif(WIN32 AND ${USE_USD_OPENVDB_BUILD} AND EXISTS ${OPENVDB_OPENEXR_BUILD})
    set(ZLIB_INSTALL_DIR ${OPENVDB_INSTALL_DIR} CACHE PATH "" FORCE)                                                                                
  endif()
  mark_as_advanced(ZLIB_INSTALL_DIR)
  
  if(EXISTS "${ZLIB_INSTALL_DIR}/release")
    set(ZLIB_INSTALL_DIR_RELEASE "${ZLIB_INSTALL_DIR}/release")
  else()
    set(ZLIB_INSTALL_DIR_RELEASE "${ZLIB_INSTALL_DIR}")
  endif()
  if(EXISTS "${ZLIB_INSTALL_DIR}/debug")
    set(ZLIB_INSTALL_DIR_DEBUG "${ZLIB_INSTALL_DIR}/debug")
  else()
    set(ZLIB_INSTALL_DIR_DEBUG "${ZLIB_INSTALL_DIR}")
  endif()
  
  unset(ZLIB_LIB_FILE_RELEASE CACHE)
  unset(ZLIB_LIB_FILE_DEBUG CACHE)
  if (WIN32)
    find_library(ZLIB_LIB_FILE_RELEASE zlibstat PATHS "${ZLIB_INSTALL_DIR_RELEASE}/lib")
    if(EXISTS ${ZLIB_LIB_FILE_RELEASE})
      find_library(ZLIB_LIB_FILE_DEBUG zlibstat PATHS "${ZLIB_INSTALL_DIR_DEBUG}/lib" REQUIRED)
    else()
      find_library(ZLIB_LIB_FILE_RELEASE zlibstatic PATHS "${ZLIB_INSTALL_DIR_RELEASE}/lib" REQUIRED)
      find_library(ZLIB_LIB_FILE_DEBUG zlibstaticd PATHS "${ZLIB_INSTALL_DIR_DEBUG}/lib" REQUIRED)
    endif()
  else()
    find_library(ZLIB_LIB_FILE_RELEASE libz.a PATHS "${ZLIB_INSTALL_DIR_RELEASE}/lib" REQUIRED)
    find_library(ZLIB_LIB_FILE_DEBUG libz.a PATHS "${ZLIB_INSTALL_DIR_DEBUG}/lib" REQUIRED)
  endif()
  message(STATUS "Zlib lib files: ${ZLIB_LIB_FILE_RELEASE}, ${ZLIB_LIB_FILE_DEBUG}")
  set(ZLIB_LIB_FILE "$<$<NOT:$<CONFIG:Debug>>:${ZLIB_LIB_FILE_RELEASE}>$<$<CONFIG:Debug>:${ZLIB_LIB_FILE_DEBUG}>")

  # All OVDB dep lib dirs #

  if(CMAKE_BUILD_TYPE MATCHES "Debug")
    set(OPENVDB_LIB_DIR "${OPENVDB_INSTALL_DIR_DEBUG}/lib")
    set(OPENEXR_LIB_DIR "${OPENEXR_INSTALL_DIR_DEBUG}/lib")
    set(BLOSC_LIB_DIR "${BLOSC_INSTALL_DIR_DEBUG}/lib")
    set(ZLIB_LIB_DIR "${ZLIB_INSTALL_DIR_DEBUG}/lib")
  else()
    set(OPENVDB_LIB_DIR "${OPENVDB_INSTALL_DIR_RELEASE}/lib")
    set(OPENEXR_LIB_DIR "${OPENEXR_INSTALL_DIR_RELEASE}/lib")
    set(BLOSC_LIB_DIR "${BLOSC_INSTALL_DIR_RELEASE}/lib")
    set(ZLIB_LIB_DIR "${ZLIB_INSTALL_DIR_RELEASE}/lib")
  endif()
  
endif()


####################
# UsdBridgeVolume #
####################

set(USDBRIDGE_VOLUME_SOURCES 
  Volume/UsdBridgeVolumeWriter.cpp
  Volume/UsdBridgeVolumeWriter.h
  PARENT_SCOPE)
  
if(${USD_DEVICE_USE_OPENVDB})
  set(USDBRIDGE_VOLUME_INCLUDE_DIRS
    ${OPENVDB_INCLUDE_DIR} ${OPENEXR_INCLUDE_DIR}
    PARENT_SCOPE)
  set(USDBRIDGE_VOLUME_COMPILE_DEFS USE_OPENVDB OPENVDB_STATICLIB PARENT_SCOPE)#OPENVDB_STATICLIB required by OpenVDB headers
  set(USDBRIDGE_VOLUME_LINK_LIBS
    ${OPENVDB_LIB_FILE} ${ILMBASE_HALF_LIB_FILE} ${BLOSC_LIB_FILE} ${ZLIB_LIB_FILE}
    PARENT_SCOPE)
    
  if (WIN32)
  else()  
    set(USDBRIDGE_VOLUME_LINK_OPTIONS
      "-Wl,--rpath-link,${OPENVDB_LIB_DIR}"
      "-Wl,--rpath-link,${OPENEXR_LIB_DIR}"
      "-Wl,--rpath-link,${BLOSC_LIB_DIR}"
      "-Wl,--rpath-link,${ZLIB_LIB_DIR}"
      PARENT_SCOPE)
  endif()
endif()

message( "-------------------- End UsdBridgeVolume CmakeLists.txt ----------------------------------------")