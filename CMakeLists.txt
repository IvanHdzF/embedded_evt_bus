cmake_minimum_required(VERSION 3.16)

project(evt_bus
  VERSION 0.1.0
  LANGUAGES C
)

# ---------------------------------------------------------------------------
# Options
# ---------------------------------------------------------------------------
option(EVT_BUS_BUILD_SHARED "Build evt_bus as a shared library" OFF)
option(EVT_BUS_BUILD_TESTS  "Build unit tests (Unity)"          ON)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# ---------------------------------------------------------------------------
# Library target
# ---------------------------------------------------------------------------
set(EVT_BUS_SOURCES
  src/evt_bus_core.c
  src/evt_bus_port.c
)

if (EVT_BUS_BUILD_SHARED)
  add_library(evt_bus SHARED ${EVT_BUS_SOURCES})
else()
  add_library(evt_bus STATIC ${EVT_BUS_SOURCES})
endif()

add_library(evt_bus::evt_bus ALIAS evt_bus)

target_include_directories(evt_bus
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include>
    $<INSTALL_INTERFACE:include>
)


target_compile_options(evt_bus PRIVATE
  -Wall -Wextra -Wpedantic
)

# If you want consumers to be able to override these at compile time:
# target_compile_definitions(evt_bus PUBLIC EVT_INLINE_MAX=16 EVT_BUS_MAX_EVT_IDS=64)

# ---------------------------------------------------------------------------
# Install (optional but nice if you want to reuse in other repos)
# ---------------------------------------------------------------------------
include(GNUInstallDirs)

install(TARGETS evt_bus
  EXPORT evt_busTargets
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(DIRECTORY include/
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(EXPORT evt_busTargets
  FILE evt_busTargets.cmake
  NAMESPACE evt_bus::
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/evt_bus
)

# ---------------------------------------------------------------------------
# Tests (Unity)
# ---------------------------------------------------------------------------
if (EVT_BUS_BUILD_TESTS)
  enable_testing()

  # Build Unity as a tiny static lib
  add_library(unity STATIC
    ${CMAKE_CURRENT_LIST_DIR}/externals/unity/src/unity.c
  )

  target_include_directories(unity PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/externals/unity/src
  )

  target_compile_options(unity PRIVATE -Wall -Wextra -Wpedantic)

  # Unit test executable
  add_executable(test_evt_bus
    tests/test_evt_bus.c
    tests/fake_evt_bus_backend.c
  )

  target_link_libraries(test_evt_bus PRIVATE
    evt_bus
    unity
  )

  target_include_directories(test_evt_bus PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/include
    ${CMAKE_CURRENT_LIST_DIR}/tests
  )

  add_test(NAME evt_bus COMMAND test_evt_bus)
endif()
