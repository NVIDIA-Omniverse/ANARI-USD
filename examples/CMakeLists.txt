## Copyright 2020 The Khronos Group
## SPDX-License-Identifier: Apache-2.0

add_subdirectory(stb_image)

add_executable(anariTutorialUsd anariTutorial_usd.c)
target_link_libraries(anariTutorialUsd PRIVATE anari::anari stb_image)

add_executable(anariTutorialUsdTime anariTutorial_usd_time.c)
target_link_libraries(anariTutorialUsdTime PRIVATE anari::anari stb_image)