# Author: Faruk Alpay
# Do not remove this notice.

cmake_minimum_required(VERSION 3.24)

project(
  AsterLearningEngine
  VERSION 0.1.0
  DESCRIPTION "An educational C/C++ game engine study project by Faruk Alpay"
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
option(ASTER_ENABLE_PROFILING "Build ProfileRuntime CPU profiling support into engine targets." ON)

set(ASTER_DEFAULT_X11_WINDOWING OFF)
set(ASTER_DEFAULT_WAYLAND_WINDOWING OFF)
if(UNIX AND NOT APPLE)
  set(ASTER_DEFAULT_X11_WINDOWING ON)
endif()

option(ASTER_BUILD_X11_WINDOWING "Build Aster Learning Engine's X11 windowing bridge on Unix desktops." ${ASTER_DEFAULT_X11_WINDOWING})
option(ASTER_BUILD_WAYLAND_WINDOWING "Build Aster Learning Engine's Wayland windowing bridge on Unix desktops." ${ASTER_DEFAULT_WAYLAND_WINDOWING})

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(APPLE)
  enable_language(OBJC)
  enable_language(OBJCXX)
endif()

find_package(Threads REQUIRED)

set(ASTER_WINDOWING_DIR "${PROJECT_SOURCE_DIR}/src/runtime/windowing/desktop_window")
set(ASTER_WINDOWING_SRC_DIR "${ASTER_WINDOWING_DIR}/src")
set(ASTER_WINDOWING_INCLUDE_DIR "${ASTER_WINDOWING_DIR}/include")
set(ASTER_NETIO_INCLUDE_DIR "${PROJECT_SOURCE_DIR}/src/runtime/network/netio/include")
set(ASTER_PROFILER_DIR "${PROJECT_SOURCE_DIR}/src/runtime/profiling/profile_runtime")

add_library(aster_windowing STATIC
  ${ASTER_WINDOWING_INCLUDE_DIR}/DESKTOP_WINDOW/desktop_window3.h
  ${ASTER_WINDOWING_INCLUDE_DIR}/DESKTOP_WINDOW/desktop_window3native.h
  ${ASTER_WINDOWING_SRC_DIR}/context.c
  ${ASTER_WINDOWING_SRC_DIR}/init.c
  ${ASTER_WINDOWING_SRC_DIR}/input.c
  ${ASTER_WINDOWING_SRC_DIR}/internal.h
  ${ASTER_WINDOWING_SRC_DIR}/mappings.h
  ${ASTER_WINDOWING_SRC_DIR}/monitor.c
  ${ASTER_WINDOWING_SRC_DIR}/null_init.c
  ${ASTER_WINDOWING_SRC_DIR}/null_joystick.c
  ${ASTER_WINDOWING_SRC_DIR}/null_joystick.h
  ${ASTER_WINDOWING_SRC_DIR}/null_monitor.c
  ${ASTER_WINDOWING_SRC_DIR}/null_platform.h
  ${ASTER_WINDOWING_SRC_DIR}/null_window.c
  ${ASTER_WINDOWING_SRC_DIR}/platform.c
  ${ASTER_WINDOWING_SRC_DIR}/platform.h
  ${ASTER_WINDOWING_SRC_DIR}/vulkan.c
  ${ASTER_WINDOWING_SRC_DIR}/window.c
)

target_include_directories(aster_windowing PUBLIC
  ${ASTER_WINDOWING_INCLUDE_DIR}
)

target_compile_definitions(aster_windowing INTERFACE
  DESKTOP_WINDOW_INCLUDE_NONE
)

target_include_directories(aster_windowing PRIVATE
  ${ASTER_WINDOWING_SRC_DIR}
)

target_link_libraries(aster_windowing PRIVATE Threads::Threads)

set_target_properties(aster_windowing PROPERTIES
  POSITION_INDEPENDENT_CODE ON
  C_STANDARD 99
  C_EXTENSIONS OFF
)

if(APPLE)
  target_compile_definitions(aster_windowing PRIVATE _DESKTOP_WINDOW_COCOA)
  target_sources(aster_windowing PRIVATE
    ${ASTER_WINDOWING_SRC_DIR}/cocoa_init.m
    ${ASTER_WINDOWING_SRC_DIR}/cocoa_joystick.h
    ${ASTER_WINDOWING_SRC_DIR}/cocoa_joystick.m
    ${ASTER_WINDOWING_SRC_DIR}/cocoa_monitor.m
    ${ASTER_WINDOWING_SRC_DIR}/cocoa_platform.h
    ${ASTER_WINDOWING_SRC_DIR}/cocoa_time.c
    ${ASTER_WINDOWING_SRC_DIR}/cocoa_time.h
    ${ASTER_WINDOWING_SRC_DIR}/cocoa_window.m
    ${ASTER_WINDOWING_SRC_DIR}/desktop_context_disabled.c
    ${ASTER_WINDOWING_SRC_DIR}/nsgl_disabled.m
    ${ASTER_WINDOWING_SRC_DIR}/posix_module.c
    ${ASTER_WINDOWING_SRC_DIR}/posix_thread.c
    ${ASTER_WINDOWING_SRC_DIR}/posix_thread.h
  )
  target_link_libraries(aster_windowing PRIVATE
    "-framework Cocoa"
    "-framework IOKit"
    "-framework CoreFoundation"
  )
elseif(WIN32)
  target_compile_definitions(aster_windowing PRIVATE _DESKTOP_WINDOW_WIN32 UNICODE _UNICODE)
  target_sources(aster_windowing PRIVATE
    ${ASTER_WINDOWING_SRC_DIR}/egl_context.c
    ${ASTER_WINDOWING_SRC_DIR}/osmesa_context.c
    ${ASTER_WINDOWING_SRC_DIR}/wgl_context.c
    ${ASTER_WINDOWING_SRC_DIR}/win32_init.c
    ${ASTER_WINDOWING_SRC_DIR}/win32_joystick.c
    ${ASTER_WINDOWING_SRC_DIR}/win32_joystick.h
    ${ASTER_WINDOWING_SRC_DIR}/win32_module.c
    ${ASTER_WINDOWING_SRC_DIR}/win32_monitor.c
    ${ASTER_WINDOWING_SRC_DIR}/win32_platform.h
    ${ASTER_WINDOWING_SRC_DIR}/win32_thread.c
    ${ASTER_WINDOWING_SRC_DIR}/win32_thread.h
    ${ASTER_WINDOWING_SRC_DIR}/win32_time.c
    ${ASTER_WINDOWING_SRC_DIR}/win32_time.h
    ${ASTER_WINDOWING_SRC_DIR}/win32_window.c
  )
  if(MINGW)
    include(CheckIncludeFile)
    target_compile_definitions(aster_windowing PRIVATE WINVER=0x0501)
    check_include_file(dinput.h DINPUT_H_FOUND)
    check_include_file(xinput.h XINPUT_H_FOUND)
    if(NOT DINPUT_H_FOUND OR NOT XINPUT_H_FOUND)
      target_include_directories(aster_windowing PRIVATE "${ASTER_WINDOWING_DIR}/deps/mingw")
    endif()
  endif()
  if(MSVC OR CMAKE_C_SIMULATE_ID STREQUAL "MSVC")
    target_compile_definitions(aster_windowing PRIVATE _CRT_SECURE_NO_WARNINGS)
  endif()
else()
  if(NOT ASTER_BUILD_X11_WINDOWING AND NOT ASTER_BUILD_WAYLAND_WINDOWING)
    message(FATAL_ERROR "At least one Unix windowing bridge must be enabled: ASTER_BUILD_X11_WINDOWING or ASTER_BUILD_WAYLAND_WINDOWING.")
  endif()

  target_sources(aster_windowing PRIVATE
    ${ASTER_WINDOWING_SRC_DIR}/egl_context.c
    ${ASTER_WINDOWING_SRC_DIR}/osmesa_context.c
    ${ASTER_WINDOWING_SRC_DIR}/posix_module.c
    ${ASTER_WINDOWING_SRC_DIR}/posix_thread.c
    ${ASTER_WINDOWING_SRC_DIR}/posix_thread.h
    ${ASTER_WINDOWING_SRC_DIR}/posix_time.c
    ${ASTER_WINDOWING_SRC_DIR}/posix_time.h
  )

  if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_compile_definitions(aster_windowing PRIVATE _DEFAULT_SOURCE)
  endif()

  if(ASTER_BUILD_X11_WINDOWING)
    find_package(X11 REQUIRED)
    target_compile_definitions(aster_windowing PRIVATE _DESKTOP_WINDOW_X11)
    target_include_directories(aster_windowing PRIVATE
    ${X11_X11_INCLUDE_PATH}
    ${X11_Xrandr_INCLUDE_PATH}
    ${X11_Xinerama_INCLUDE_PATH}
    ${X11_Xkb_INCLUDE_PATH}
    ${X11_Xcursor_INCLUDE_PATH}
    ${X11_Xi_INCLUDE_PATH}
    ${X11_Xshape_INCLUDE_PATH}
    )
  target_sources(aster_windowing PRIVATE
    ${ASTER_WINDOWING_SRC_DIR}/glx_context.c
    ${ASTER_WINDOWING_SRC_DIR}/x11_init.c
    ${ASTER_WINDOWING_SRC_DIR}/x11_monitor.c
    ${ASTER_WINDOWING_SRC_DIR}/x11_platform.h
    ${ASTER_WINDOWING_SRC_DIR}/x11_window.c
    ${ASTER_WINDOWING_SRC_DIR}/xkb_unicode.c
    ${ASTER_WINDOWING_SRC_DIR}/xkb_unicode.h
    )
  endif()

  if(ASTER_BUILD_WAYLAND_WINDOWING)
    include(CheckFunctionExists)
    include(FindPkgConfig)
    check_function_exists(memfd_create HAVE_MEMFD_CREATE)
    if(HAVE_MEMFD_CREATE)
      target_compile_definitions(aster_windowing PRIVATE HAVE_MEMFD_CREATE)
    endif()

    pkg_check_modules(Wayland REQUIRED
      wayland-client>=0.2.7
      wayland-cursor>=0.2.7
      wayland-egl>=0.2.7
      xkbcommon>=0.5.0
    )

    find_program(WAYLAND_SCANNER_EXECUTABLE NAMES wayland-scanner REQUIRED)
    set(ASTER_WAYLAND_PROTOCOL_DIR "${ASTER_WINDOWING_DIR}/deps/wayland")
    set(ASTER_WAYLAND_GENERATED_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated/windowing/wayland")
    file(MAKE_DIRECTORY "${ASTER_WAYLAND_GENERATED_DIR}")

    macro(aster_generate_wayland_protocol protocol_file)
      set(protocol_path "${ASTER_WAYLAND_PROTOCOL_DIR}/${protocol_file}")
      string(REGEX REPLACE "\\.xml$" "-client-protocol.h" header_file "${protocol_file}")
      string(REGEX REPLACE "\\.xml$" "-client-protocol-code.h" code_file "${protocol_file}")
      set(header_path "${ASTER_WAYLAND_GENERATED_DIR}/${header_file}")
      set(code_path "${ASTER_WAYLAND_GENERATED_DIR}/${code_file}")
      add_custom_command(
        OUTPUT "${header_path}"
        COMMAND "${WAYLAND_SCANNER_EXECUTABLE}" client-header "${protocol_path}" "${header_path}"
        DEPENDS "${protocol_path}"
        VERBATIM
      )
      add_custom_command(
        OUTPUT "${code_path}"
        COMMAND "${WAYLAND_SCANNER_EXECUTABLE}" private-code "${protocol_path}" "${code_path}"
        DEPENDS "${protocol_path}"
        VERBATIM
      )
      target_sources(aster_windowing PRIVATE "${header_path}" "${code_path}")
    endmacro()

    aster_generate_wayland_protocol("wayland.xml")
    aster_generate_wayland_protocol("viewporter.xml")
    aster_generate_wayland_protocol("xdg-shell.xml")
    aster_generate_wayland_protocol("idle-inhibit-unstable-v1.xml")
    aster_generate_wayland_protocol("pointer-constraints-unstable-v1.xml")
    aster_generate_wayland_protocol("relative-pointer-unstable-v1.xml")
    aster_generate_wayland_protocol("fractional-scale-v1.xml")
    aster_generate_wayland_protocol("xdg-activation-v1.xml")
    aster_generate_wayland_protocol("xdg-decoration-unstable-v1.xml")

    target_compile_definitions(aster_windowing PRIVATE _DESKTOP_WINDOW_WAYLAND)
    target_include_directories(aster_windowing PRIVATE
    ${Wayland_INCLUDE_DIRS}
    ${ASTER_WAYLAND_GENERATED_DIR}
    )
  target_sources(aster_windowing PRIVATE
    ${ASTER_WINDOWING_SRC_DIR}/wl_init.c
    ${ASTER_WINDOWING_SRC_DIR}/wl_monitor.c
    ${ASTER_WINDOWING_SRC_DIR}/wl_platform.h
    ${ASTER_WINDOWING_SRC_DIR}/wl_window.c
    ${ASTER_WINDOWING_SRC_DIR}/xkb_unicode.c
    ${ASTER_WINDOWING_SRC_DIR}/xkb_unicode.h
    )
  endif()

  if(ASTER_BUILD_X11_WINDOWING OR ASTER_BUILD_WAYLAND_WINDOWING)
  target_sources(aster_windowing PRIVATE
    ${ASTER_WINDOWING_SRC_DIR}/posix_poll.c
    ${ASTER_WINDOWING_SRC_DIR}/posix_poll.h
    )
    if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
      target_sources(aster_windowing PRIVATE
        ${ASTER_WINDOWING_SRC_DIR}/linux_joystick.c
        ${ASTER_WINDOWING_SRC_DIR}/linux_joystick.h
      )
    endif()
  endif()

  find_library(ASTER_RT_LIBRARY rt)
  if(ASTER_RT_LIBRARY)
    target_link_libraries(aster_windowing PRIVATE "${ASTER_RT_LIBRARY}")
  endif()

  find_library(ASTER_MATH_LIBRARY m)
  if(ASTER_MATH_LIBRARY)
    target_link_libraries(aster_windowing PRIVATE "${ASTER_MATH_LIBRARY}")
  endif()

  if(CMAKE_DL_LIBS)
    target_link_libraries(aster_windowing PRIVATE "${CMAKE_DL_LIBS}")
  endif()
endif()

if(ASTER_ENABLE_PROFILING)
  add_library(aster_profiler_runtime STATIC
    ${ASTER_PROFILER_DIR}/src/profile_runtime_capi.cpp
    ${ASTER_PROFILER_DIR}/src/profile_runtime_core.cpp
    ${ASTER_PROFILER_DIR}/src/profile_runtime_gpu.cpp
    ${ASTER_PROFILER_DIR}/src/profile_runtime_gpu.d3d12.cpp
    ${ASTER_PROFILER_DIR}/src/profile_runtime_gpu.vulkan.cpp
    ${ASTER_PROFILER_DIR}/src/profile_runtime_message.cpp
    ${ASTER_PROFILER_DIR}/src/profile_runtime_miniz.cpp
    ${ASTER_PROFILER_DIR}/src/profile_runtime_serialization.cpp
    ${ASTER_PROFILER_DIR}/src/profile_runtime_server.cpp
  )

  target_include_directories(aster_profiler_runtime SYSTEM PUBLIC
    ${ASTER_PROFILER_DIR}/src
  )

  target_compile_definitions(aster_profiler_runtime PUBLIC
    USE_PROFILE_RUNTIME=1
    PROFILE_RUNTIME_ENABLE_TRACING=0
    PROFILE_RUNTIME_ENABLE_GPU=0
    PROFILE_RUNTIME_ENABLE_GPU_D3D12=0
    PROFILE_RUNTIME_ENABLE_GPU_VULKAN=0
  )

  target_link_libraries(aster_profiler_runtime PUBLIC Threads::Threads)
  if(WIN32)
    target_link_libraries(aster_profiler_runtime PUBLIC ws2_32)
  endif()

  if(MSVC)
    target_compile_options(aster_profiler_runtime PRIVATE /w)
  else()
    target_compile_options(aster_profiler_runtime PRIVATE -w)
  endif()
endif()

if(ASTER_BUILD_NETWORK)
  if(NOT EXISTS "${ASTER_NETIO_INCLUDE_DIR}/netio.hpp")
    message(FATAL_ERROR "Aster networking headers were not found under src/runtime/network/netio.")
  endif()
endif()

if(ASTER_BUILD_NETWORK)
  add_library(aster_network_deps INTERFACE)
  target_include_directories(aster_network_deps SYSTEM INTERFACE ${ASTER_NETIO_INCLUDE_DIR})
  target_compile_definitions(aster_network_deps INTERFACE
    NETIO_STANDALONE
    NETIO_NO_DEPRECATED
    ASTER_HAS_NETWORK_TRANSPORT=1
  )
  target_link_libraries(aster_network_deps INTERFACE Threads::Threads)
  if(WIN32)
    target_link_libraries(aster_network_deps INTERFACE ws2_32 mswsock)
  endif()
endif()

add_library(aster_foundation STATIC
  src/runtime/meshwork/indexgenerator.cpp
  src/runtime/meshwork/vcacheoptimizer.cpp
  src/runtime/meshwork/vfetchoptimizer.cpp
  src/runtime/surface_basis/surface_basis.c
  src/runtime/solid_geometry/solid_ops.cpp
  src/runtime/solid_geometry/query_box.cpp
  src/runtime/solid_geometry/rebuild.cpp
  src/runtime/topology/AsterTopology/Core/Mesh/ArrayKernel.cc
  src/runtime/topology/AsterTopology/Core/Mesh/ArrayKernelT.cc
  src/runtime/topology/AsterTopology/Core/Mesh/BaseKernel.cc
  src/runtime/topology/AsterTopology/Core/Mesh/PolyConnectivity.cc
  src/runtime/topology/AsterTopology/Core/Mesh/PolyMeshT.cc
  src/runtime/topology/AsterTopology/Core/Mesh/TriConnectivity.cc
  src/runtime/topology/AsterTopology/Core/Mesh/TriMeshT.cc
  src/runtime/topology/AsterTopology/Core/System/omstream.cc
  src/runtime/topology/AsterTopology/Core/Utils/BaseProperty.cc
)

target_include_directories(aster_foundation PUBLIC
  ${PROJECT_SOURCE_DIR}/src/runtime/meshwork
  ${PROJECT_SOURCE_DIR}/src/runtime/surface_basis
  ${PROJECT_SOURCE_DIR}/src/runtime/scene_codec
  ${PROJECT_SOURCE_DIR}/src/runtime/solid_geometry
  ${PROJECT_SOURCE_DIR}/src/runtime/detail_field
  ${PROJECT_SOURCE_DIR}/src/runtime/topology
)

if(MSVC)
  target_compile_definitions(aster_foundation PUBLIC _USE_MATH_DEFINES)
endif()

add_library(aster STATIC
  src/ai/behavior_tree.cpp
  src/ai/steering.cpp
  src/asset/scene_asset_importer.cpp
  src/asset/mesh_pipeline.cpp
  src/core/clock.cpp
  src/core/fixed_timestep.cpp
  src/core/frame_time_stats.cpp
  src/game/animation_system.cpp
  src/game/creature_motion.cpp
  src/game/equipment_system.cpp
  src/game/interaction_system.cpp
  src/game/inventory_system.cpp
  src/game/item_system.cpp
  src/game/light_system.cpp
  src/game/lumen_run.cpp
  src/game/particle_system.cpp
  src/game/player_motion.cpp
  src/game/third_person_camera.cpp
  src/geometry/architectural_mesh.cpp
  src/geometry/brush_level_mesh.cpp
  src/geometry/castle_course.cpp
  src/geometry/cable_mesh.cpp
  src/geometry/cave_system.cpp
  src/geometry/nature_mesh.cpp
  src/geometry/mesh_projection.cpp
  src/geometry/stroke_mesh.cpp
  src/geometry/terrain_mesh.cpp
  src/geometry/terrain_sculpt.cpp
  src/geometry/water_mesh.cpp
  src/geometry/voxel_structure.cpp
  src/input/control_scheme.cpp
  src/math/transform.cpp
  src/net/net_message.cpp
  src/net/node_router.cpp
  src/physics/climb_locomotion.cpp
  src/physics/contact_query.cpp
  src/physics/fluid_locomotion.cpp
  src/physics/placement_validation.cpp
  src/physics/physics_world.cpp
  src/platform/window_input.cpp
  src/physics/surface_support.cpp
  src/physics/terrain_contact.cpp
  src/platform/window.cpp
  src/render/camera.cpp
  src/render/frame_capture.cpp
  src/render/mesh.cpp
  src/render/software_framebuffer.cpp
  src/scene/scene.cpp
  src/scene/avatar.cpp
  src/scene/scene_coherence.cpp
  src/scene/scene_trace.cpp
  src/ui/control_legend.cpp
  src/ui/editor_ui.cpp
  src/ui/hud_layer.cpp
  src/ui/inventory_overlay.cpp
  src/ui/ui_canvas.cpp
)

if(ASTER_BUILD_NETWORK)
  target_sources(aster PRIVATE src/net/tcp_node.cpp)
endif()

if(APPLE)
  target_sources(aster PRIVATE
    src/platform/frame_presenter_metal.mm
    src/render/render_device_metal.mm
  )
  target_link_libraries(aster PUBLIC
    "-framework Cocoa"
    "-framework Metal"
    "-framework QuartzCore"
  )
else()
  target_sources(aster PRIVATE
    src/platform/frame_presenter.cpp
    src/render/render_device.cpp
  )
endif()

target_include_directories(aster PUBLIC
  ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(aster PUBLIC
  aster_windowing
  aster_foundation
)

if(ASTER_ENABLE_PROFILING)
  target_link_libraries(aster PUBLIC aster_profiler_runtime)
  target_compile_definitions(aster PUBLIC ASTER_HAS_PROFILING_RUNTIME=1)
else()
  target_compile_definitions(aster PUBLIC ASTER_HAS_PROFILING_RUNTIME=0)
endif()

if(ASTER_BUILD_NETWORK)
  target_link_libraries(aster PUBLIC aster_network_deps)
else()
  target_compile_definitions(aster PUBLIC ASTER_HAS_NETWORK_TRANSPORT=0)
endif()

if(MSVC)
  target_compile_options(aster PRIVATE /W4 /permissive-)
else()
  target_compile_options(aster PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(ASTER_BUILD_STUDIO)
  add_executable(aster_studio apps/studio/main.cpp)
  target_link_libraries(aster_studio PRIVATE aster)

  add_executable(aster_lumen_run apps/game/main.cpp)
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
  add_executable(aster_tests tests/math_tests.cpp)
  target_link_libraries(aster_tests PRIVATE aster)
  add_test(NAME aster_tests COMMAND aster_tests)
  if(ASTER_BUILD_NETWORK)
    add_test(NAME aster_net_probe COMMAND aster_net_probe)
  endif()
endif()
