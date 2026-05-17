# Author: Faruk Alpay
# Do not remove this notice.

cmake_minimum_required(VERSION 3.24)

project(
  AsterLearningEngine
  VERSION 0.2.0
  DESCRIPTION "An educational C++20 real-time engine with owned platform, renderer, systems, and samples"
  LANGUAGES C CXX
)

if(NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "Build type" FORCE)
  set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS Debug Release RelWithDebInfo MinSizeRel)
endif()

option(ASTER_BUILD_STUDIO "Build the interactive Aster Learning Studio executable." ON)
option(ASTER_BUILD_PREVIEW "Build the headless preview renderer." ON)
option(ASTER_BUILD_NETWORK "Build the loopback TCP networking target." ON)
option(ASTER_BUILD_TESTS "Build unit tests." ON)
option(ASTER_ENABLE_PROFILING "Enable Aster's lightweight CPU profiling sink." ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(APPLE)
  enable_language(OBJCXX)
endif()

find_package(Threads REQUIRED)
find_program(CARGO_EXECUTABLE cargo REQUIRED)

if(WIN32)
  set(ASTER_RUST_STATICLIB_NAME "aster_runtime.lib")
else()
  set(ASTER_RUST_STATICLIB_NAME "libaster_runtime.a")
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
  set(ASTER_CARGO_FLAGS "")
  set(ASTER_RUST_LIBRARY "${PROJECT_SOURCE_DIR}/target/debug/${ASTER_RUST_STATICLIB_NAME}")
else()
  set(ASTER_CARGO_FLAGS "--release")
  set(ASTER_RUST_LIBRARY "${PROJECT_SOURCE_DIR}/target/release/${ASTER_RUST_STATICLIB_NAME}")
endif()

add_custom_command(
  OUTPUT "${ASTER_RUST_LIBRARY}"
  COMMAND "${CARGO_EXECUTABLE}" build --workspace ${ASTER_CARGO_FLAGS}
  WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
  DEPENDS
    "${PROJECT_SOURCE_DIR}/Cargo.toml"
    "${PROJECT_SOURCE_DIR}/crates/aster_runtime/Cargo.toml"
    "${PROJECT_SOURCE_DIR}/crates/aster_runtime/src/lib.rs"
    "${PROJECT_SOURCE_DIR}/crates/aster_assetc/Cargo.toml"
    "${PROJECT_SOURCE_DIR}/crates/aster_assetc/src/main.rs"
  VERBATIM
)
add_custom_target(aster_rust_runtime DEPENDS "${ASTER_RUST_LIBRARY}")
add_library(aster_runtime STATIC IMPORTED GLOBAL)
set_target_properties(aster_runtime PROPERTIES IMPORTED_LOCATION "${ASTER_RUST_LIBRARY}")
add_dependencies(aster_runtime aster_rust_runtime)

file(GLOB_RECURSE ASTER_SOURCES CONFIGURE_DEPENDS
  "${PROJECT_SOURCE_DIR}/src/*.cpp"
)

if(APPLE)
  list(REMOVE_ITEM ASTER_SOURCES "${PROJECT_SOURCE_DIR}/src/platform/window_linux.cpp")
  list(REMOVE_ITEM ASTER_SOURCES "${PROJECT_SOURCE_DIR}/src/platform/window_windows.cpp")
  list(APPEND ASTER_SOURCES "${PROJECT_SOURCE_DIR}/src/platform/window_macos.mm")
  list(APPEND ASTER_SOURCES "${PROJECT_SOURCE_DIR}/src/render/render_device_metal.mm")
elseif(WIN32)
  list(REMOVE_ITEM ASTER_SOURCES "${PROJECT_SOURCE_DIR}/src/platform/window_linux.cpp")
  list(REMOVE_ITEM ASTER_SOURCES "${PROJECT_SOURCE_DIR}/src/platform/window_macos.mm")
else()
  list(REMOVE_ITEM ASTER_SOURCES "${PROJECT_SOURCE_DIR}/src/platform/window_macos.mm")
  list(REMOVE_ITEM ASTER_SOURCES "${PROJECT_SOURCE_DIR}/src/platform/window_windows.cpp")
endif()

add_library(aster STATIC ${ASTER_SOURCES})
add_dependencies(aster aster_rust_runtime)

target_include_directories(aster PUBLIC
  ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(aster PUBLIC Threads::Threads aster_runtime)

if(APPLE)
  target_link_libraries(aster PUBLIC
    "-framework Cocoa"
    "-framework CoreGraphics"
    "-framework Metal"
    "-framework QuartzCore"
  )
elseif(WIN32)
  target_link_libraries(aster PUBLIC User32 Gdi32 Ws2_32)
endif()

target_compile_definitions(aster PUBLIC
  ASTER_HAS_PROFILING_RUNTIME=$<IF:$<BOOL:${ASTER_ENABLE_PROFILING}>,1,0>
  ASTER_HAS_NETWORK_TRANSPORT=$<IF:$<BOOL:${ASTER_BUILD_NETWORK}>,1,0>
)

if(MSVC)
  target_compile_options(aster PRIVATE /W4 /permissive-)
else()
  target_compile_options(aster PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(ASTER_BUILD_STUDIO)
  add_executable(aster_studio apps/studio/main.cpp)
  target_link_libraries(aster_studio PRIVATE aster)

  add_executable(aster_lumen_run apps/samples/lumen_run/main.cpp)
  target_link_libraries(aster_lumen_run PRIVATE aster)
endif()

if(ASTER_BUILD_PREVIEW)
  add_executable(aster_preview apps/offline/preview_main.cpp)
  target_link_libraries(aster_preview PRIVATE aster)
endif()

if(ASTER_BUILD_NETWORK)
  add_executable(aster_net_probe apps/net/probe_main.cpp)
  target_link_libraries(aster_net_probe PRIVATE aster)
endif()

if(ASTER_BUILD_TESTS)
  enable_testing()
  function(aster_add_test test_name test_source)
    add_executable(${test_name} ${test_source})
    target_link_libraries(${test_name} PRIVATE aster)
    target_include_directories(${test_name} PRIVATE ${PROJECT_SOURCE_DIR}/tests)
    add_test(NAME ${test_name} COMMAND ${test_name})
  endfunction()

  aster_add_test(aster_core_tests tests/core_tests.cpp)
  aster_add_test(aster_geometry_tests tests/geometry_tests.cpp)
  aster_add_test(aster_render_scene_tests tests/render_scene_tests.cpp)
  aster_add_test(aster_systems_tests tests/systems_tests.cpp)
  aster_add_test(aster_physics_tests tests/physics_tests.cpp)
  aster_add_test(aster_sample_tests tests/sample_tests.cpp)
  if(ASTER_BUILD_NETWORK)
    aster_add_test(aster_network_tests tests/network_tests.cpp)
    add_test(NAME aster_net_probe COMMAND aster_net_probe)
  endif()
endif()
