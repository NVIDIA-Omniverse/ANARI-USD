message( "-------------------- Begin UsdBridgeCommon CmakeLists.txt ----------------------------------------")

project(UsdBridge_Common)
add_library(${PROJECT_NAME} INTERFACE)

target_sources(${PROJECT_NAME}
  INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/UsdBridgeData.h
    ${CMAKE_CURRENT_LIST_DIR}/UsdBridgeMacros.h
    ${CMAKE_CURRENT_LIST_DIR}/UsdBridgeNumerics.h
    ${CMAKE_CURRENT_LIST_DIR}/UsdBridgeUtils_Internal.h
    ${CMAKE_CURRENT_LIST_DIR}/UsdBridgeUtils.cpp
    ${CMAKE_CURRENT_LIST_DIR}/UsdBridgeUtils.h
)

target_include_directories(${PROJECT_NAME} INTERFACE ${CMAKE_CURRENT_LIST_DIR})

message( "-------------------- End UsdBridgeCommon CmakeLists.txt ----------------------------------------")

