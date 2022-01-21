function(FIND_DEBUG_LIBRARIES LIBRARIES_DEBUG LIBRARIES_RELEASE DEBUG_SEARCH_PATH)
  # Manually find the debug library from a possibly different build directory
  message(STATUS "Running find_library on ${DEBUG_SEARCH_PATH}")
  foreach(RELEASE_LIB ${LIBRARIES_RELEASE})
    get_filename_component(LIBRARY_FILE_NAME ${RELEASE_LIB} NAME_WE)
    get_filename_component(LIBRARY_FILE_EXT ${RELEASE_LIB} EXT)
    
    set(LIBRARY_NAMES_DEBUG 
      "${LIBRARY_FILE_NAME}d${LIBRARY_FILE_EXT}"
      "${LIBRARY_FILE_NAME}_d${LIBRARY_FILE_EXT}"
      "${LIBRARY_FILE_NAME}${LIBRARY_FILE_EXT}"
      "${LIBRARY_FILE_NAME}_s${LIBRARY_FILE_EXT}"
      "${LIBRARY_FILE_NAME}staticd${LIBRARY_FILE_EXT}"
      "${LIBRARY_FILE_NAME}_s_d${LIBRARY_FILE_EXT}")
  
    unset(LIBRARY_FIND_RESULT CACHE)
    find_library(LIBRARY_FIND_RESULT 
      NAMES ${LIBRARY_NAMES_DEBUG} 
      PATHS ${DEBUG_SEARCH_PATH}
      REQUIRED)
    
    list(APPEND LIBRARIES_DEBUG_LIST ${LIBRARY_FIND_RESULT})
  endforeach()
  set(${LIBRARIES_DEBUG} ${LIBRARIES_DEBUG_LIST} PARENT_SCOPE)
endfunction()

message( "-------------------- Begin UsdBridgeVolume CmakeLists.txt ----------------------------------------")

option(USD_DEVICE_USE_OPENVDB "Enable OpenVDB support for USD device" OFF)

project(UsdBridge_Volume)

add_library(${PROJECT_NAME} INTERFACE)
target_sources(${PROJECT_NAME}
INTERFACE
  ${CMAKE_CURRENT_LIST_DIR}/UsdBridgeVolumeWriter.cpp
  ${CMAKE_CURRENT_LIST_DIR}/UsdBridgeVolumeWriter.h
)

if (USD_DEVICE_USE_OPENVDB)

  file(GLOB USD_OPENVDB_BUILD "${USD_INSTALL_DIR_RELEASE}/include/openvdb")
  option(USE_USD_OPENVDB_BUILD "If USD source installation exists, use OpenVDB from there" ON)
  
  # Support custom install dir OPENVDB_INSTALL_DIR
  
  if(${USE_USD_OPENVDB_BUILD} AND EXISTS ${USD_OPENVDB_BUILD})
    set(OPENVDB_INSTALL_DIR ${USD_INSTALL_DIR} CACHE PATH "" FORCE)
  endif()
  
  if (NOT EXISTS ${OPENVDB_INSTALL_DIR})
    find_path(OPENVDB_INSTALL_DIR NAMES include/openvdb/openvdb.h)
    message(STATUS "Using OPENVDB_INSTALL_DIR: ${OPENVDB_INSTALL_DIR}")
  endif()
  
  if (NOT EXISTS ${OPENVDB_INSTALL_DIR})
    message(FATAL_ERROR "No valid OPENVDB_INSTALL_DIR set, or found using CMAKE_PREFIX_PATH: ${OPENVDB_INSTALL_DIR}")
  endif()

  MESSAGE(STATUS "OpenVDB enabled. In case superbuild disabled BUILD_USD, it's taken from OPENVDB_INSTALL_DIR: ${OPENVDB_INSTALL_DIR}")

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

  # Add OpenVDB cmake to CMAKE_MODULE_PATH

  unset(OPENVDB_MODULE_DIR CACHE)
 
  find_path(OPENVDB_MODULE_DIR
    PATHS
      ${OPENVDB_INSTALL_DIR_DEBUG}
    NAMES
      FindOpenVDB.cmake
    PATH_SUFFIXES
      OpenVDB
      cmake/OpenVDB
      lib/cmake/OpenVDB
  )
  
  list(APPEND CMAKE_MODULE_PATH ${OPENVDB_MODULE_DIR})
  
  
  ###
  # FindOpenVDB.cmake tries to pull in irrelevant Boost components, so just manually fetch its dependencies instead.
  
  set(OPENVDB_LIB_DIR_DEBUG "${OPENVDB_INSTALL_DIR_DEBUG}/lib")
  
  # IlmBase 
  
  set(ILMBASE_ROOT "${OPENVDB_INSTALL_DIR_RELEASE}")
  find_package(IlmBase REQUIRED COMPONENTS Half)
  
  set(ILMBASE_LIBRARIES_RELEASE ${IlmBase_LIBRARIES})
  unset(ILMBASE_LIBRARIES_DEBUG CACHE)
  find_debug_libraries(
    ILMBASE_LIBRARIES_DEBUG 
    ${ILMBASE_LIBRARIES_RELEASE} 
    ${OPENVDB_LIB_DIR_DEBUG}
    )
  
  # Zlib
  
  set(ZLIB_ROOT "${OPENVDB_INSTALL_DIR_RELEASE}" "${OPENVDB_INSTALL_DIR_DEBUG}")
  find_package(ZLIB REQUIRED)
  
  # Blosc
  
  set(BLOSC_ROOT "${OPENVDB_INSTALL_DIR_RELEASE}")
  find_package(Blosc REQUIRED)
  
  set(BLOSC_LIBRARIES_RELEASE ${Blosc_LIBRARIES})
  unset(BLOSC_LIBRARIES_DEBUG CACHE)
  find_debug_libraries(
    BLOSC_LIBRARIES_DEBUG 
    ${BLOSC_LIBRARIES_RELEASE} 
    ${OPENVDB_LIB_DIR_DEBUG}
    )
  
  # OpenVDB
  
  # Dynamic library fails to link fully on Windows, but FindOpenVDB.cmake can't properly select the static library. Do it manually instead.
  if (WIN32)
    set(OPENVDB_LIB_FILE_NAME "libopenvdb.lib")
  else()
    set(OPENVDB_LIB_FILE_NAME "libopenvdb.a")
  endif()
  find_library(OPENVDB_LIBRARY_RELEASE ${OPENVDB_LIB_FILE_NAME} PATHS "${OPENVDB_INSTALL_DIR_RELEASE}/lib" REQUIRED)
  find_library(OPENVDB_LIBRARY_DEBUG ${OPENVDB_LIB_FILE_NAME} PATHS "${OPENVDB_INSTALL_DIR_DEBUG}/lib" REQUIRED)
  
  # End of manual dependency fetch
  ###
  
  
  # Set debug and release variables
  
  set(OPENVDB_INCLUDE_DIRS_RELEASE "${OPENVDB_INSTALL_DIR_RELEASE}/include")
  set(OPENVDB_LIB_FILES_RELEASE 
    ${OPENVDB_LIBRARY_RELEASE}
    ${ILMBASE_LIBRARIES_RELEASE} 
    ${ZLIB_LIBRARY_RELEASE} 
    ${BLOSC_LIBRARIES_RELEASE}
    )
  
  set(OPENVDB_INCLUDE_DIRS_DEBUG "${OPENVDB_INSTALL_DIR_DEBUG}/include")
  set(OPENVDB_LIB_FILES_DEBUG 
    ${OPENVDB_LIBRARY_DEBUG}
    ${ILMBASE_LIBRARIES_DEBUG} 
    ${ZLIB_LIBRARY_DEBUG} 
    ${BLOSC_LIBRARIES_DEBUG}
    )
  
  set(OPENVDB_INCLUDE_DIRS "$<$<NOT:$<CONFIG:Debug>>:${OPENVDB_INCLUDE_DIRS_RELEASE}>$<$<CONFIG:Debug>:${OPENVDB_INCLUDE_DIRS_DEBUG}>")
  set(OPENVDB_LIB_FILES "$<$<NOT:$<CONFIG:Debug>>:${OPENVDB_LIB_FILES_RELEASE}>$<$<CONFIG:Debug>:${OPENVDB_LIB_FILES_DEBUG}>")

  message(STATUS "Using OpenVDB include dirs: ${OPENVDB_INCLUDE_DIRS}")
  message(STATUS "Using OpenVDB libs: ${OPENVDB_LIB_FILES}")
  
  # Add include dirs and link libs
  
  target_include_directories(${PROJECT_NAME} INTERFACE ${OPENVDB_INCLUDE_DIRS})
  target_link_libraries(${PROJECT_NAME} INTERFACE ${OPENVDB_LIB_FILES})
  target_compile_definitions(${PROJECT_NAME}
    INTERFACE
        USE_OPENVDB
        OPENVDB_STATICLIB
  )
endif()

message( "-------------------- End UsdBridgeVolume CmakeLists.txt ----------------------------------------")
