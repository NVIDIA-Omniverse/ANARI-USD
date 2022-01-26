message( "-------------------- Begin UsdBridgeConnect CmakeLists.txt ----------------------------------------")

option(USD_DEVICE_USE_OMNIVERSE "Enable Omniverse support for USD device" OFF)

####################
# Omniverse Client #
####################

if(${USD_DEVICE_USE_OMNIVERSE})
    
    if (NOT EXISTS ${OMNICLIENT_INSTALL_DIR})
      find_path(OMNICLIENT_INSTALL_DIR NAMES include/OmniClient.h DOC "Path to Omniverse Client")
      message(STATUS "Using OMNICLIENT_INSTALL_DIR: ${OMNICLIENT_INSTALL_DIR}")
    endif()
    
    if (NOT EXISTS ${OMNICLIENT_INSTALL_DIR})
      message(FATAL_ERROR "No valid OMNICLIENT_INSTALL_DIR set, or found using CMAKE_PREFIX_PATH: ${OMNICLIENT_INSTALL_DIR}")
    endif()

    if(EXISTS ${OMNICLIENT_INSTALL_DIR})   
      MESSAGE(STATUS "OmniConnect is using OmniClient at: ${OMNICLIENT_INSTALL_DIR}")
      
      set(OMNICLIENT_DIR_RELEASE "${OMNICLIENT_INSTALL_DIR}/release")
      set(OMNICLIENT_DIR_DEBUG "${OMNICLIENT_INSTALL_DIR}/debug")
    
      get_filename_component(USD_EXT_PACKAGE_DIR_RELEASE ${OMNICLIENT_DIR_RELEASE} ABSOLUTE)
      get_filename_component(USD_EXT_PACKAGE_DIR_DEBUG ${OMNICLIENT_DIR_DEBUG} ABSOLUTE)
      
      set(OMNICLIENT_INCLUDE_DIR "${OMNICLIENT_INSTALL_DIR}/include")
      set(OMNICLIENT_LIB_DIR_RELEASE "${OMNICLIENT_DIR_RELEASE}")
      set(OMNICLIENT_LIB_DIR_DEBUG   "${OMNICLIENT_DIR_DEBUG}")
      
      if (CMAKE_CONFIGURATION_TYPES)
        set(OMNICLIENT_LIB_DIR "$<$<NOT:$<CONFIG:Debug>>:${OMNICLIENT_LIB_DIR_RELEASE}>$<$<CONFIG:Debug>:${OMNICLIENT_LIB_DIR_DEBUG}>")
      else() 
        if(CMAKE_BUILD_TYPE MATCHES "Debug")
          set(OMNICLIENT_LIB_DIR ${OMNICLIENT_LIB_DIR_DEBUG})
        else()
          set(OMNICLIENT_LIB_DIR ${OMNICLIENT_LIB_DIR_RELEASE})
        endif()
      endif()
    
      list(APPEND OMNICLIENT_SHARED_LIBS omniclient)
      MESSAGE(STATUS "OMNICLIENT_SHARED_LIBS: ${OMNICLIENT_SHARED_LIBS}")
    else() 
      MESSAGE(FATAL_ERROR "Cannot find OMNICLIENT_INSTALL_DIR: ${OMNICLIENT_INSTALL_DIR}")   
    endif()
else()
    MESSAGE(STATUS "OmniConnect is NOT using OmniClient for remote connections.")
endif()

####################
# UsdBridgeConnect #
####################

set(USDBRIDGE_CONNECT_SOURCES 
  Connection/UsdBridgeConnection.cpp
  Connection/UsdBridgeConnection.h
  PARENT_SCOPE)

if(${USD_DEVICE_USE_OMNIVERSE})
  # Set include and lib dirs
  set(USDBRIDGE_CONNECT_INCLUDE_DIRS ${OMNICLIENT_INCLUDE_DIR} PARENT_SCOPE)
  set(USDBRIDGE_CONNECT_COMPILE_DEFS OMNIVERSE_CONNECTION_ENABLE PARENT_SCOPE)
  
  if (CMAKE_CONFIGURATION_TYPES)
    set(USDBRIDGE_CONNECT_LINK_DIRS ${OMNICLIENT_LIB_DIR} PARENT_SCOPE)
    set(USDBRIDGE_CONNECT_LINK_LIBS ${OMNICLIENT_SHARED_LIBS} PARENT_SCOPE)
  else()
    set(USDBRIDGE_CONNECT_LINK_OPTIONS 
      "$<$<CONFIG:Debug>:-Wl,--rpath-link,${OMNICLIENT_LIB_DIR_DEBUG}>"
      "$<$<NOT:$<CONFIG:Debug>>:-Wl,--rpath-link,${OMNICLIENT_LIB_DIR_RELEASE}>"
      PARENT_SCOPE)
      
    foreach(lib ${OMNICLIENT_SHARED_LIBS})
      find_library(path ${lib} PATHS ${OMNICLIENT_LIB_DIR} NO_DEFAULT_PATH REQUIRED)
      list(APPEND OMNI_LIBS ${path})
      unset(path CACHE)
    endforeach()

    set(USDBRIDGE_CONNECT_LINK_LIBS ${OMNI_LIBS} PARENT_SCOPE)
  endif()
  
  # Install client library
  set(PLATFORM_INSTALL_LIBDIR "$<$<NOT:$<PLATFORM_ID:Windows>>:${CMAKE_INSTALL_LIBDIR}>$<$<PLATFORM_ID:Windows>:${CMAKE_INSTALL_BINDIR}>/")
  install(
    DIRECTORY "${OMNICLIENT_LIB_DIR}/"
    DESTINATION ${PLATFORM_INSTALL_LIBDIR}
    PATTERN "*${CMAKE_STATIC_LIBRARY_SUFFIX}" EXCLUDE)
    
endif()  

message( "-------------------- End UsdBridgeConnect CmakeLists.txt ----------------------------------------")

