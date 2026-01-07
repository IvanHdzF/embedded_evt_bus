# ports/esp-idf/CMakeLists.txt

# This file is meant to be included from an ESP-IDF component CMakeLists.txt.
# Example wrapper (in the ESP-IDF app):
#   include(${CMAKE_CURRENT_LIST_DIR}/../../externals/embedded_evt_bus/ports/esp-idf/CMakeLists.txt)

get_filename_component(EVT_BUS_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)

idf_component_register(
  SRCS
    "${EVT_BUS_ROOT}/src/evt_bus_core.c"
    "${EVT_BUS_ROOT}/ports/freertos/evt_bus_port_freertos.c"
  INCLUDE_DIRS
    "${EVT_BUS_ROOT}/include"
    "${EVT_BUS_ROOT}/ports/freertos"
  REQUIRES
    freertos
)
