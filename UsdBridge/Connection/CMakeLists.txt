message( "-------------------- Begin UsdBridgeConnect CmakeLists.txt ----------------------------------------")

option(USD_DEVICE_USE_OMNIVERSE "Enable Omniverse support for USD device" OFF)

####################
# Omniverse Client #
####################

if(${USD_DEVICE_USE_OMNIVERSE})
    set(OMNI_CLIENT_INSTALL_DIR "" CACHE PATH "Path to Omniverse Client")
    mark_as_advanced(OMNI_CLIENT_INSTALL_DIR)
    
    if (NOT OMNI_CLIENT_INSTALL_DIR)
      find_path(OMNI_CLIENT_INSTALL_DIR NAMES include/OmniClient.h)
      message(STATUS "Using OMNI_CLIENT_INSTALL_DIR: ${OMNI_CLIENT_INSTALL_DIR}")
    endif()

    if(EXISTS ${OMNI_CLIENT_INSTALL_DIR})   
      MESSAGE(STATUS "OmniConnect is using OmniClient at: ${OMNI_CLIENT_INSTALL_DIR}")
      
      set(OMNI_CLIENT_DIR_RELEASE "${OMNI_CLIENT_INSTALL_DIR}/release")
      set(OMNI_CLIENT_DIR_DEBUG "${OMNI_CLIENT_INSTALL_DIR}/debug")
    
      get_filename_component(USD_EXT_PACKAGE_DIR_RELEASE ${OMNI_CLIENT_DIR_RELEASE} ABSOLUTE)
      get_filename_component(USD_EXT_PACKAGE_DIR_DEBUG ${OMNI_CLIENT_DIR_DEBUG} ABSOLUTE)
      
      set(OMNI_CLIENT_INCLUDE_DIR "${OMNI_CLIENT_INSTALL_DIR}/include")
      set(OMNI_CLIENT_LIB_DIR_RELEASE "${OMNI_CLIENT_DIR_RELEASE}")
      set(OMNI_CLIENT_LIB_DIR_DEBUG   "${OMNI_CLIENT_DIR_DEBUG}")
      
      if (WIN32)
        set(OMNI_CLIENT_LIB_DIR "$<$<NOT:$<CONFIG:Debug>>:${OMNI_CLIENT_LIB_DIR_RELEASE}>$<$<CONFIG:Debug>:${OMNI_CLIENT_LIB_DIR_DEBUG}>")
      else() 
        if(CMAKE_BUILD_TYPE MATCHES "Debug")
          set(OMNI_CLIENT_LIB_DIR ${OMNI_CLIENT_LIB_DIR_DEBUG})
        else()
          set(OMNI_CLIENT_LIB_DIR ${OMNI_CLIENT_LIB_DIR_RELEASE})
        endif()
      endif()
    
      list(APPEND OMNI_CLIENT_SHARED_LIBS omniclient)
      MESSAGE(STATUS "OMNI_CLIENT_SHARED_LIBS: ${OMNI_CLIENT_SHARED_LIBS}")
    else() 
      MESSAGE(FATAL_ERROR "Cannot find OMNI_CLIENT_INSTALL_DIR: ${OMNI_CLIENT_INSTALL_DIR}")   
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
  set(USDBRIDGE_CONNECT_INCLUDE_DIRS ${OMNI_CLIENT_INCLUDE_DIR} PARENT_SCOPE)
  set(USDBRIDGE_CONNECT_COMPILE_DEFS OMNIVERSE_CONNECTION_ENABLE PARENT_SCOPE)
  
  if (WIN32)
    set(USDBRIDGE_CONNECT_LINK_DIRS ${OMNI_CLIENT_LIB_DIR} PARENT_SCOPE)
    set(USDBRIDGE_CONNECT_LINK_LIBS ${OMNI_CLIENT_SHARED_LIBS} PARENT_SCOPE)
  else()
    set(USDBRIDGE_CONNECT_LINK_OPTIONS 
      "$<$<CONFIG:Debug>:-Wl,--rpath-link,${OMNI_CLIENT_LIB_DIR_DEBUG}>"
      "$<$<NOT:$<CONFIG:Debug>>:-Wl,--rpath-link,${OMNI_CLIENT_LIB_DIR_RELEASE}>"
      PARENT_SCOPE)
      
    foreach(lib ${OMNI_CLIENT_SHARED_LIBS})
      find_library(path ${lib} PATHS ${OMNI_CLIENT_LIB_DIR} REQUIRED)
      list(APPEND OMNI_LIBS ${path})
      unset(path CACHE)
    endforeach()

    set(USDBRIDGE_CONNECT_LINK_LIBS ${OMNI_LIBS} PARENT_SCOPE)
  endif()
endif()  

message( "-------------------- End UsdBridgeConnect CmakeLists.txt ----------------------------------------")

