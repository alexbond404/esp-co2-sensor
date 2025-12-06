# For more information about build system see
# https://docs.espressif.com/projects/esp-idf/en/latest/api-guides/build-system.html
# The following five lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.5)

set(PROJECT_VER "0.1.0")
set(PROJECT_TITLE "esp-co2-sensor")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
idf_build_component("${CMAKE_CURRENT_LIST_DIR}/ha_mqtt")
project(${PROJECT_TITLE})
