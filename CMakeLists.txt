cmake_minimum_required(VERSION 3.16)

project(evt_bus
  VERSION 0.1.0
  LANGUAGES C
)

# ---------------------------------------------------------------------------
# Options
# ---------------------------------------------------------------------------
option(EVT_BUS_BUILD_TESTS       "Build unit tests (Unity)" OFF)
option(EVT_BUS_ENABLE_FREERTOS   "Build FreeRTOS port"      OFF)
option(EVT_BUS_FREERTOS_STUB     "Use stub FreeRTOS headers to compile port" OFF)

# Provided by user when EVT_BUS_ENABLE_FREERTOS=ON and STUB=OFF:
#   -DFREERTOS_INCLUDE_DIRS="path1;path2;..."
set(FREERTOS_INCLUDE_DIRS "" CACHE STRING "FreeRTOS include dirs (semicolon-separated)")
set(FREERTOS_LIBS         "" CACHE STRING "Optional FreeRTOS libs/targets to link")

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# ---------------------------------------------------------------------------
# Core library (RTOS-agnostic)
# ---------------------------------------------------------------------------
add_library(evt_bus_core STATIC
  src/evt_bus_core.c
)
add_library(evt_bus::core ALIAS evt_bus_core)

target_include_directories(evt_bus_core
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_compile_options(evt_bus_core PRIVATE
  -Wall -Wextra -Wpedantic
)

# ---------------------------------------------------------------------------
# Public "evt_bus" target (wrapper)
# Consumers link evt_bus::evt_bus and get core + selected port.
# ---------------------------------------------------------------------------
add_library(evt_bus INTERFACE)
add_library(evt_bus::evt_bus ALIAS evt_bus)

target_link_libraries(evt_bus INTERFACE evt_bus_core)
target_include_directories(evt_bus INTERFACE
  $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include>
  $<INSTALL_INTERFACE:include>
)

# ---------------------------------------------------------------------------
# FreeRTOS port (optional)
# ---------------------------------------------------------------------------
if(EVT_BUS_ENABLE_FREERTOS)
  add_library(evt_bus_port_freertos STATIC
    ports/freertos/evt_bus_port_freertos.c
  )
  add_library(evt_bus::freertos ALIAS evt_bus_port_freertos)

  target_link_libraries(evt_bus_port_freertos PUBLIC evt_bus_core)

  target_include_directories(evt_bus_port_freertos PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/ports/freertos>
  )

  target_compile_options(evt_bus_port_freertos PRIVATE
    -Wall -Wextra -Wpedantic
  )

  if(EVT_BUS_FREERTOS_STUB)
    target_include_directories(evt_bus_port_freertos PUBLIC
      ${CMAKE_CURRENT_LIST_DIR}/tests/freertos_stub
    )
  else()
    if(FREERTOS_INCLUDE_DIRS STREQUAL "")
      message(FATAL_ERROR
        "FREERTOS_INCLUDE_DIRS not set. Provide FREERTOS_INC=... or enable EVT_BUS_FREERTOS_STUB."
      )
    endif()
    target_include_directories(evt_bus_port_freertos PUBLIC ${FREERTOS_INCLUDE_DIRS})

    if(DEFINED FREERTOS_LIBS)
      target_link_libraries(evt_bus_port_freertos PUBLIC ${FREERTOS_LIBS})
    endif()
  endif()

  # Pull port into public wrapper
  target_link_libraries(evt_bus INTERFACE evt_bus_port_freertos)
endif()

# ---------------------------------------------------------------------------
# Tests (Unity)
# ---------------------------------------------------------------------------
if (EVT_BUS_BUILD_TESTS)
  enable_testing()

  add_library(unity STATIC
    ${CMAKE_CURRENT_LIST_DIR}/externals/unity/src/unity.c
  )
  target_include_directories(unity PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/externals/unity/src
  )
  target_compile_options(unity PRIVATE -Wall -Wextra -Wpedantic)

  add_executable(test_evt_bus
    tests/test_evt_bus.c
    tests/fake_evt_bus_backend.c
  )

  # IMPORTANT: link to core, not the public wrapper (which may pull ports)
  target_link_libraries(test_evt_bus PRIVATE
    evt_bus_core
    unity
  )

  target_include_directories(test_evt_bus PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/include
    ${CMAKE_CURRENT_LIST_DIR}/tests
  )

  add_test(NAME evt_bus COMMAND test_evt_bus)
endif()
