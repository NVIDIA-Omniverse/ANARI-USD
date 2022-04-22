## Copyright 2020 The Khronos Group
## SPDX-License-Identifier: Apache-2.0

add_subdirectory(stb_image)

project(anariTutorialUsd)
add_executable(${PROJECT_NAME} anariTutorial_usd.c)
target_link_libraries(${PROJECT_NAME} PRIVATE anari::anari stb_image)
install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

project(anariTutorialUsdTime)
add_executable(${PROJECT_NAME} anariTutorial_usd_time.c)
target_link_libraries(${PROJECT_NAME} PRIVATE anari::anari stb_image)
install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

project(anariTutorialUsdRecreate)
add_executable(${PROJECT_NAME} anariTutorial_usd_recreate.c)
target_link_libraries(${PROJECT_NAME} PRIVATE anari::anari stb_image)
install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})