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
include(GNUInstallDirs)

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
if(WIN32)
  set_target_properties(aster_runtime PROPERTIES
    INTERFACE_LINK_LIBRARIES "Advapi32;Bcrypt;Ntdll;Userenv;Ws2_32"
  )
endif()
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

  find_package(PkgConfig REQUIRED)
  pkg_check_modules(WAYLAND_CLIENT REQUIRED IMPORTED_TARGET wayland-client)
  pkg_check_modules(WAYLAND_PROTOCOLS REQUIRED wayland-protocols)
  pkg_get_variable(WAYLAND_PROTOCOLS_PKGDATADIR wayland-protocols pkgdatadir)
  find_program(WAYLAND_SCANNER_EXECUTABLE wayland-scanner REQUIRED)
  if(NOT WAYLAND_PROTOCOLS_PKGDATADIR)
    message(FATAL_ERROR "wayland-protocols did not report a pkgdatadir.")
  endif()

  set(ASTER_WAYLAND_PROTOCOL_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated/wayland")
  file(MAKE_DIRECTORY "${ASTER_WAYLAND_PROTOCOL_DIR}")

  function(aster_generate_wayland_protocol protocol_name protocol_xml)
    if(NOT EXISTS "${protocol_xml}")
      message(FATAL_ERROR "Missing Wayland protocol XML: ${protocol_xml}")
    endif()
    set(protocol_header "${ASTER_WAYLAND_PROTOCOL_DIR}/${protocol_name}-client-protocol.h")
    set(protocol_code "${ASTER_WAYLAND_PROTOCOL_DIR}/${protocol_name}-protocol.c")
    add_custom_command(
      OUTPUT "${protocol_header}"
      COMMAND "${WAYLAND_SCANNER_EXECUTABLE}" client-header "${protocol_xml}" "${protocol_header}"
      DEPENDS "${protocol_xml}"
      VERBATIM
    )
    add_custom_command(
      OUTPUT "${protocol_code}"
      COMMAND "${WAYLAND_SCANNER_EXECUTABLE}" private-code "${protocol_xml}" "${protocol_code}"
      DEPENDS "${protocol_xml}"
      VERBATIM
    )
    list(APPEND ASTER_SOURCES "${protocol_code}" "${protocol_header}")
    set(ASTER_SOURCES "${ASTER_SOURCES}" PARENT_SCOPE)
  endfunction()

  aster_generate_wayland_protocol(
    xdg-shell
    "${WAYLAND_PROTOCOLS_PKGDATADIR}/stable/xdg-shell/xdg-shell.xml"
  )
  aster_generate_wayland_protocol(
    relative-pointer-unstable-v1
    "${WAYLAND_PROTOCOLS_PKGDATADIR}/unstable/relative-pointer/relative-pointer-unstable-v1.xml"
  )
  aster_generate_wayland_protocol(
    pointer-constraints-unstable-v1
    "${WAYLAND_PROTOCOLS_PKGDATADIR}/unstable/pointer-constraints/pointer-constraints-unstable-v1.xml"
  )
endif()

list(REMOVE_ITEM ASTER_SOURCES "${PROJECT_SOURCE_DIR}/src/kernel/kernel.cpp")

add_library(aster_engine_internal STATIC ${ASTER_SOURCES})
add_library(aster ALIAS aster_engine_internal)
add_dependencies(aster_engine_internal aster_rust_runtime)

target_include_directories(aster_engine_internal PUBLIC
  ${PROJECT_SOURCE_DIR}/include
)
if(UNIX AND NOT APPLE)
  target_include_directories(aster_engine_internal PRIVATE ${ASTER_WAYLAND_PROTOCOL_DIR})
endif()

target_link_libraries(aster_engine_internal PUBLIC Threads::Threads aster_runtime)
if(UNIX AND NOT APPLE)
  target_link_libraries(aster_engine_internal PRIVATE PkgConfig::WAYLAND_CLIENT)
endif()

if(APPLE)
  target_link_libraries(aster_engine_internal PUBLIC
    "-framework Cocoa"
    "-framework CoreGraphics"
    "-framework Metal"
    "-framework QuartzCore"
  )
elseif(WIN32)
  target_link_libraries(aster_engine_internal PUBLIC User32 Gdi32 Ws2_32)
endif()

target_compile_definitions(aster_engine_internal PUBLIC
  ASTER_HAS_PROFILING_RUNTIME=$<IF:$<BOOL:${ASTER_ENABLE_PROFILING}>,1,0>
  ASTER_HAS_NETWORK_TRANSPORT=$<IF:$<BOOL:${ASTER_BUILD_NETWORK}>,1,0>
)

if(MSVC)
  target_compile_options(aster_engine_internal PRIVATE /W4 /permissive-)
else()
  target_compile_options(aster_engine_internal PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_library(aster_kernel SHARED src/kernel/kernel.cpp)
add_library(aster::kernel ALIAS aster_kernel)
target_include_directories(aster_kernel PUBLIC
  $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
target_compile_definitions(aster_kernel PRIVATE ASTER_KERNEL_BUILD=1)
set_target_properties(aster_kernel PROPERTIES
  EXPORT_NAME kernel
  VERSION ${PROJECT_VERSION}
  SOVERSION 1
  CXX_VISIBILITY_PRESET hidden
  VISIBILITY_INLINES_HIDDEN YES
)
if(MSVC)
  target_compile_options(aster_kernel PRIVATE /W4 /permissive-)
else()
  target_compile_options(aster_kernel PRIVATE -Wall -Wextra -Wpedantic)
endif()

install(TARGETS aster_kernel
  EXPORT AsterKernelTargets
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)
install(DIRECTORY include/aster/kernel DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/aster)
install(FILES abi/aster_kernel.symbols DESTINATION ${CMAKE_INSTALL_DATADIR}/aster/abi)
install(EXPORT AsterKernelTargets
  NAMESPACE aster::
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/AsterKernel
)

if(ASTER_BUILD_STUDIO)
  add_executable(aster_studio apps/studio/main.cpp)
  target_link_libraries(aster_studio PRIVATE aster_engine_internal)

  add_executable(aster_lumen_run apps/samples/lumen_run/main.cpp)
  target_link_libraries(aster_lumen_run PRIVATE aster_engine_internal)
endif()

if(ASTER_BUILD_PREVIEW)
  add_executable(aster_preview apps/offline/preview_main.cpp)
  target_link_libraries(aster_preview PRIVATE aster_engine_internal)
endif()

if(ASTER_BUILD_NETWORK)
  add_executable(aster_net_probe apps/net/probe_main.cpp)
  target_link_libraries(aster_net_probe PRIVATE aster_engine_internal)
endif()

if(ASTER_BUILD_TESTS)
  enable_testing()
  function(aster_add_test test_name test_source)
    add_executable(${test_name} ${test_source})
    target_link_libraries(${test_name} PRIVATE aster_engine_internal)
    target_include_directories(${test_name} PRIVATE ${PROJECT_SOURCE_DIR}/tests)
    add_test(NAME ${test_name} COMMAND ${test_name})
  endfunction()

  aster_add_test(aster_core_tests tests/core_tests.cpp)
  aster_add_test(aster_geometry_tests tests/geometry_tests.cpp)
  add_executable(aster_kernel_public_consumer tests/kernel_public_consumer.cpp)
  target_link_libraries(aster_kernel_public_consumer PRIVATE aster_kernel)
  add_test(NAME aster_kernel_public_consumer COMMAND aster_kernel_public_consumer)

  add_executable(aster_kernel_contract_tests tests/kernel_contract_tests.cpp)
  target_link_libraries(aster_kernel_contract_tests PRIVATE aster_kernel)
  target_compile_definitions(aster_kernel_contract_tests PRIVATE
    ASTER_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
  )
  add_test(NAME aster_kernel_contract_tests COMMAND aster_kernel_contract_tests)

  find_program(ASTER_NM_EXECUTABLE NAMES llvm-nm nm)
  if(ASTER_NM_EXECUTABLE AND NOT WIN32)
    add_test(NAME aster_kernel_symbol_manifest
      COMMAND ${CMAKE_COMMAND}
        -DASTER_NM_EXECUTABLE=${ASTER_NM_EXECUTABLE}
        -DASTER_KERNEL_LIBRARY=$<TARGET_FILE:aster_kernel>
        -DASTER_KERNEL_SYMBOL_MANIFEST=${PROJECT_SOURCE_DIR}/abi/aster_kernel.symbols
        -P ${PROJECT_SOURCE_DIR}/tools/check_kernel_symbols.cmake
    )
  endif()

  add_executable(aster_render_scene_tests tests/render_scene_tests.cpp)
  target_link_libraries(aster_render_scene_tests PRIVATE aster_engine_internal)
  target_include_directories(aster_render_scene_tests PRIVATE ${PROJECT_SOURCE_DIR}/tests)
  foreach(render_scene_case
      framebuffer_origin
      ui_text_fitting
      hud_visibility
      scene_contract
      industrial_pipe_scene
      software_preview_renderer
      material_render_policies
      rust_render_frame_plan
      scene_coherence_energy
      scene_trace_validation
      scene_trace_fo_model_checking
      scene_trace_fo_quantifier_rank
      scene_trace_fo_separator_solving
  )
    add_test(NAME aster_render_scene_tests.${render_scene_case}
             COMMAND aster_render_scene_tests ${render_scene_case})
  endforeach()
  aster_add_test(aster_systems_tests tests/systems_tests.cpp)
  aster_add_test(aster_physics_tests tests/physics_tests.cpp)
  aster_add_test(aster_sample_tests tests/sample_tests.cpp)
  if(ASTER_BUILD_NETWORK)
    aster_add_test(aster_network_tests tests/network_tests.cpp)
    add_test(NAME aster_net_probe COMMAND aster_net_probe)
  endif()
endif()
