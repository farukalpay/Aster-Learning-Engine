# SPDX-License-Identifier: Apache-2.0
# Copyright (c) 2026 Faruk Alpay

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
option(ASTER_ENABLE_D3D12 "Build the Direct3D 12 native renderer when the toolchain supports it." ON)
set(ASTER_ENABLE_WAYLAND "AUTO" CACHE STRING "Wayland backend selection: AUTO, ON, or OFF.")
set_property(CACHE ASTER_ENABLE_WAYLAND PROPERTY STRINGS AUTO ON OFF)
string(TOUPPER "${ASTER_ENABLE_WAYLAND}" ASTER_ENABLE_WAYLAND_MODE)
if(NOT ASTER_ENABLE_WAYLAND_MODE STREQUAL "AUTO" AND
   NOT ASTER_ENABLE_WAYLAND_MODE STREQUAL "ON" AND
   NOT ASTER_ENABLE_WAYLAND_MODE STREQUAL "OFF")
  message(FATAL_ERROR "ASTER_ENABLE_WAYLAND must be AUTO, ON, or OFF.")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(APPLE)
  enable_language(OBJCXX)
endif()

find_package(Threads REQUIRED)
find_program(CARGO_EXECUTABLE cargo REQUIRED)
include(CheckIncludeFileCXX)
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

set(ASTER_KERNEL_PACKAGE_VERSION "8.0.0")

if(WIN32 AND NOT MINGW)
  set(ASTER_RUST_STATICLIB_NAME "aster_runtime.lib")
else()
  set(ASTER_RUST_STATICLIB_NAME "libaster_runtime.a")
endif()

set(ASTER_RUST_TARGET "" CACHE STRING "Cargo target triple used for cross-compiling Rust artifacts.")
if(NOT ASTER_RUST_TARGET AND MINGW)
  if(CMAKE_CXX_COMPILER MATCHES "x86_64.*mingw")
    set(ASTER_RUST_TARGET "x86_64-pc-windows-gnu")
  elseif(CMAKE_CXX_COMPILER MATCHES "i[3-6]86.*mingw")
    set(ASTER_RUST_TARGET "i686-pc-windows-gnu")
  endif()
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
  set(ASTER_CARGO_FLAGS "")
  set(ASTER_CARGO_PROFILE "debug")
else()
  set(ASTER_CARGO_FLAGS "--release")
  set(ASTER_CARGO_PROFILE "release")
endif()
set(ASTER_CARGO_TARGET_ARGS)
if(ASTER_RUST_TARGET)
  list(APPEND ASTER_CARGO_TARGET_ARGS "--target" "${ASTER_RUST_TARGET}")
  set(ASTER_RUST_ARTIFACT_DIR "${PROJECT_SOURCE_DIR}/target/${ASTER_RUST_TARGET}/${ASTER_CARGO_PROFILE}")
else()
  set(ASTER_RUST_ARTIFACT_DIR "${PROJECT_SOURCE_DIR}/target/${ASTER_CARGO_PROFILE}")
endif()
set(ASTER_RUST_LIBRARY "${ASTER_RUST_ARTIFACT_DIR}/${ASTER_RUST_STATICLIB_NAME}")
if(WIN32)
  set(ASTER_CARGO_EXECUTABLE_SUFFIX ".exe")
else()
  set(ASTER_CARGO_EXECUTABLE_SUFFIX "")
endif()
set(ASTER_ASSETC_EXECUTABLE "${ASTER_RUST_ARTIFACT_DIR}/aster_assetc${ASTER_CARGO_EXECUTABLE_SUFFIX}")
set(ASTER_MATERIALC_EXECUTABLE "${ASTER_RUST_ARTIFACT_DIR}/aster_materialc${ASTER_CARGO_EXECUTABLE_SUFFIX}")
set(ASTER_TEXTUREC_EXECUTABLE "${ASTER_RUST_ARTIFACT_DIR}/aster_texturec${ASTER_CARGO_EXECUTABLE_SUFFIX}")
if(ASTER_RUST_TARGET)
  string(REPLACE "-" "_" ASTER_RUST_STAMP_TARGET "${ASTER_RUST_TARGET}")
else()
  set(ASTER_RUST_STAMP_TARGET "host")
endif()
set(ASTER_RUST_STAMP_DIR "${PROJECT_BINARY_DIR}/cargo/${ASTER_RUST_STAMP_TARGET}/${ASTER_CARGO_PROFILE}")
set(ASTER_RUST_STAMP "${ASTER_RUST_STAMP_DIR}/aster_rust_runtime.stamp")
set(ASTER_CARGO_ENV)
if(UNIX AND NOT APPLE)
  list(APPEND ASTER_CARGO_ENV "RUSTFLAGS=$ENV{RUSTFLAGS} -C relocation-model=pic")
endif()

add_custom_command(
  OUTPUT "${ASTER_RUST_STAMP}"
  BYPRODUCTS
    "${ASTER_RUST_LIBRARY}"
    "${ASTER_ASSETC_EXECUTABLE}"
    "${ASTER_MATERIALC_EXECUTABLE}"
    "${ASTER_TEXTUREC_EXECUTABLE}"
  COMMAND "${CMAKE_COMMAND}" -E make_directory "${ASTER_RUST_STAMP_DIR}"
  COMMAND "${CMAKE_COMMAND}" -E env ${ASTER_CARGO_ENV} "${CARGO_EXECUTABLE}" build --workspace ${ASTER_CARGO_TARGET_ARGS} ${ASTER_CARGO_FLAGS}
  COMMAND "${CMAKE_COMMAND}" -E touch "${ASTER_RUST_STAMP}"
  WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
  DEPENDS
    "${PROJECT_SOURCE_DIR}/Cargo.toml"
    "${PROJECT_SOURCE_DIR}/crates/aster_content/Cargo.toml"
    "${PROJECT_SOURCE_DIR}/crates/aster_content/src/lib.rs"
    "${PROJECT_SOURCE_DIR}/crates/aster_runtime/Cargo.toml"
    "${PROJECT_SOURCE_DIR}/crates/aster_runtime/src/lib.rs"
    "${PROJECT_SOURCE_DIR}/crates/aster_assetc/Cargo.toml"
    "${PROJECT_SOURCE_DIR}/crates/aster_assetc/src/agent_plan.rs"
    "${PROJECT_SOURCE_DIR}/crates/aster_assetc/src/main.rs"
    "${PROJECT_SOURCE_DIR}/crates/aster_assetc/src/bin/aster_materialc.rs"
    "${PROJECT_SOURCE_DIR}/crates/aster_assetc/src/bin/aster_texturec.rs"
  VERBATIM
)
add_custom_target(aster_rust_runtime DEPENDS "${ASTER_RUST_STAMP}")
add_library(aster_runtime STATIC IMPORTED GLOBAL)
set_target_properties(aster_runtime PROPERTIES IMPORTED_LOCATION "${ASTER_RUST_LIBRARY}")
if(WIN32)
  set_target_properties(aster_runtime PROPERTIES
    INTERFACE_LINK_LIBRARIES "Advapi32;Bcrypt;Ntdll;Userenv;Ws2_32"
  )
elseif(UNIX AND NOT APPLE)
  set(ASTER_RUST_NATIVE_LIBS Threads::Threads ${CMAKE_DL_LIBS} m)
  if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    list(APPEND ASTER_RUST_NATIVE_LIBS rt util)
  endif()
  set_target_properties(aster_runtime PROPERTIES
    INTERFACE_LINK_LIBRARIES "${ASTER_RUST_NATIVE_LIBS}"
  )
endif()
add_dependencies(aster_runtime aster_rust_runtime)

set(ASTER_GAME_SDK_SOURCES
  "${PROJECT_SOURCE_DIR}/src/game_sdk/agent_authoring.cpp"
  "${PROJECT_SOURCE_DIR}/src/game_sdk/game_sdk.cpp"
)

set(ASTER_LEARNING_SOURCES
  "${PROJECT_SOURCE_DIR}/src/learning/learning_runtime.cpp"
  "${PROJECT_SOURCE_DIR}/src/learning/memory_controller.cpp"
)

set(ASTER_CORE_SOURCES
  "${PROJECT_SOURCE_DIR}/src/core/clock.cpp"
  "${PROJECT_SOURCE_DIR}/src/core/belief_extraction.cpp"
  "${PROJECT_SOURCE_DIR}/src/core/deterministic_sim.cpp"
  "${PROJECT_SOURCE_DIR}/src/core/fixed_timestep.cpp"
  "${PROJECT_SOURCE_DIR}/src/core/frame_control.cpp"
  "${PROJECT_SOURCE_DIR}/src/core/frame_time_stats.cpp"
  "${PROJECT_SOURCE_DIR}/src/core/job_graph.cpp"
  "${PROJECT_SOURCE_DIR}/src/core/module_registry.cpp"
  "${PROJECT_SOURCE_DIR}/src/core/neural_irradiance_volume.cpp"
  "${PROJECT_SOURCE_DIR}/src/core/perceptual_causality_graph.cpp"
  "${PROJECT_SOURCE_DIR}/src/core/perceptual_world_runtime.cpp"
  "${PROJECT_SOURCE_DIR}/src/core/profiler.cpp"
  "${PROJECT_SOURCE_DIR}/src/core/session_journal.cpp"
  "${PROJECT_SOURCE_DIR}/src/core/typed_trace.cpp"
  "${PROJECT_SOURCE_DIR}/src/core/work_budget.cpp"
  "${PROJECT_SOURCE_DIR}/src/core/world_perception_ledger.cpp"
  "${PROJECT_SOURCE_DIR}/src/core/world_perceptual_primitive.cpp"
  "${PROJECT_SOURCE_DIR}/src/core/world_state.cpp"
  "${PROJECT_SOURCE_DIR}/src/math/transform.cpp"
)

set(ASTER_RHI_SOURCES
  "${PROJECT_SOURCE_DIR}/src/rhi/resource_registry.cpp"
  "${PROJECT_SOURCE_DIR}/src/rhi/resource_lifetime_validator.cpp"
)

set(ASTER_FRAMEGRAPH_SOURCES
  "${PROJECT_SOURCE_DIR}/src/framegraph/frame_graph.cpp"
  "${PROJECT_SOURCE_DIR}/src/framegraph/graph_compiler.cpp"
  "${PROJECT_SOURCE_DIR}/src/framegraph/transient_resource_allocator.cpp"
)

set(ASTER_GRAPHICS_CORE7_SOURCES
  "${PROJECT_SOURCE_DIR}/src/graphics_core7/backend_visual_delta.cpp"
  "${PROJECT_SOURCE_DIR}/src/graphics_core7/clustered_light_gpu_tables.cpp"
  "${PROJECT_SOURCE_DIR}/src/graphics_core7/descriptor_arena.cpp"
  "${PROJECT_SOURCE_DIR}/src/graphics_core7/frame_gate.cpp"
  "${PROJECT_SOURCE_DIR}/src/graphics_core7/gpu_visibility_compaction.cpp"
  "${PROJECT_SOURCE_DIR}/src/graphics_core7/graphics_core7.cpp"
  "${PROJECT_SOURCE_DIR}/src/graphics_core7/pipeline_cache_warmup.cpp"
  "${PROJECT_SOURCE_DIR}/src/graphics_core7/reflection_probe_atlas.cpp"
  "${PROJECT_SOURCE_DIR}/src/graphics_core7/rhi_device_contract.cpp"
  "${PROJECT_SOURCE_DIR}/src/graphics_core7/shader_reflection_db.cpp"
  "${PROJECT_SOURCE_DIR}/src/graphics_core7/shadow_atlas_native.cpp"
  "${PROJECT_SOURCE_DIR}/src/graphics_core7/temporal_resolve.cpp"
  "${PROJECT_SOURCE_DIR}/src/graphics_core7/transient_heap_allocator.cpp"
  "${PROJECT_SOURCE_DIR}/src/graphics_core7/upload_ring.cpp"
  "${PROJECT_SOURCE_DIR}/src/graphics_core7/verdict_evaluation.cpp"
  "${PROJECT_SOURCE_DIR}/src/graphics_core7/visual_delta_classifier.cpp"
  "${PROJECT_SOURCE_DIR}/src/graphics_core7/volumetric_fog_native.cpp"
)

set(ASTER_ASSET_SOURCES
  "${PROJECT_SOURCE_DIR}/src/asset/asset_database.cpp"
  "${PROJECT_SOURCE_DIR}/src/asset/asset_foundry.cpp"
  "${PROJECT_SOURCE_DIR}/src/asset/asset_io.cpp"
  "${PROJECT_SOURCE_DIR}/src/asset/asset_library.cpp"
  "${PROJECT_SOURCE_DIR}/src/asset/asset_modifier_stack.cpp"
  "${PROJECT_SOURCE_DIR}/src/asset/asset_production_model.cpp"
  "${PROJECT_SOURCE_DIR}/src/asset/asset_registry.cpp"
  "${PROJECT_SOURCE_DIR}/src/asset/compiled_scene_asset.cpp"
  "${PROJECT_SOURCE_DIR}/src/asset/construction_yard_asset.cpp"
  "${PROJECT_SOURCE_DIR}/src/asset/content_pack.cpp"
  "${PROJECT_SOURCE_DIR}/src/asset/derived_asset_cache.cpp"
  "${PROJECT_SOURCE_DIR}/src/asset/legacy_lump_archive.cpp"
  "${PROJECT_SOURCE_DIR}/src/asset/mesh_pipeline.cpp"
  "${PROJECT_SOURCE_DIR}/src/asset/pipe_runtime_asset.cpp"
  "${PROJECT_SOURCE_DIR}/src/asset/procedural_asset_graph.cpp"
  "${PROJECT_SOURCE_DIR}/src/asset/procedural_graph_runtime.cpp"
  "${PROJECT_SOURCE_DIR}/src/asset/scene_asset_importer.cpp"
)

set(ASTER_MATERIAL_SOURCES
  "${PROJECT_SOURCE_DIR}/src/material/material_asset.cpp"
  "${PROJECT_SOURCE_DIR}/src/material/material_compiler.cpp"
  "${PROJECT_SOURCE_DIR}/src/material/material_graph.cpp"
  "${PROJECT_SOURCE_DIR}/src/material/material_instance.cpp"
  "${PROJECT_SOURCE_DIR}/src/material/material_lab.cpp"
  "${PROJECT_SOURCE_DIR}/src/material/material_parameter_block.cpp"
  "${PROJECT_SOURCE_DIR}/src/material/procedural_surface.cpp"
)

set(ASTER_TEXTURE_SOURCES
  "${PROJECT_SOURCE_DIR}/src/texture/texture_asset.cpp"
  "${PROJECT_SOURCE_DIR}/src/texture/texture_atlas.cpp"
  "${PROJECT_SOURCE_DIR}/src/texture/texture_compressor.cpp"
  "${PROJECT_SOURCE_DIR}/src/texture/texture_debug.cpp"
  "${PROJECT_SOURCE_DIR}/src/texture/texture_importer.cpp"
  "${PROJECT_SOURCE_DIR}/src/texture/runtime_texture.cpp"
  "${PROJECT_SOURCE_DIR}/src/texture/texture_streamer.cpp"
)

set(ASTER_SHADER_SOURCES
  "${PROJECT_SOURCE_DIR}/src/shader/shader_archive.cpp"
  "${PROJECT_SOURCE_DIR}/src/shader/shader_compiler.cpp"
  "${PROJECT_SOURCE_DIR}/src/shader/shader_hot_reload.cpp"
  "${PROJECT_SOURCE_DIR}/src/shader/shader_library.cpp"
  "${PROJECT_SOURCE_DIR}/src/shader/shader_preprocessor.cpp"
  "${PROJECT_SOURCE_DIR}/src/shader/shader_reflection.cpp"
  "${PROJECT_SOURCE_DIR}/src/shader/shader_variant.cpp"
)

set(ASTER_GAMEPLAY_SOURCES
  "${PROJECT_SOURCE_DIR}/src/gameplay/action_scheduler.cpp"
  "${PROJECT_SOURCE_DIR}/src/gameplay/feature_labels.cpp"
)

set(ASTER_RENDER_SOURCES
  "${PROJECT_SOURCE_DIR}/src/render/aster.cpp"
  "${PROJECT_SOURCE_DIR}/src/render/camera.cpp"
  "${PROJECT_SOURCE_DIR}/src/render/frame_capture.cpp"
  "${PROJECT_SOURCE_DIR}/src/render/material_compiler.cpp"
  "${PROJECT_SOURCE_DIR}/src/render/mesh.cpp"
  "${PROJECT_SOURCE_DIR}/src/render/native_render_backend_null.cpp"
  "${PROJECT_SOURCE_DIR}/src/render/render_asset_cache.cpp"
  "${PROJECT_SOURCE_DIR}/src/render/render_backend_common.cpp"
  "${PROJECT_SOURCE_DIR}/src/render/render_device.cpp"
  "${PROJECT_SOURCE_DIR}/src/render/render_graph.cpp"
  "${PROJECT_SOURCE_DIR}/src/render/render_graph_executor.cpp"
  "${PROJECT_SOURCE_DIR}/src/render/render_quality.cpp"
  "${PROJECT_SOURCE_DIR}/src/render/render_scene.cpp"
  "${PROJECT_SOURCE_DIR}/src/render/preview_compositor.cpp"
  "${PROJECT_SOURCE_DIR}/src/render/software_framebuffer.cpp"
  "${PROJECT_SOURCE_DIR}/src/render/software_preview_renderer.cpp"
  "${PROJECT_SOURCE_DIR}/src/render/visual_regression.cpp"
)

set(ASTER_HAS_D3D12_BACKEND OFF)
if(WIN32 AND MSVC AND ASTER_ENABLE_D3D12)
  check_include_file_cxx(d3d12.h ASTER_HAS_D3D12_HEADER)
  check_include_file_cxx(dxgi1_6.h ASTER_HAS_DXGI_HEADER)
  check_include_file_cxx(d3dcompiler.h ASTER_HAS_D3DCOMPILER_HEADER)
  if(ASTER_HAS_D3D12_HEADER AND ASTER_HAS_DXGI_HEADER AND ASTER_HAS_D3DCOMPILER_HEADER)
    set(ASTER_HAS_D3D12_BACKEND ON)
    set(ASTER_D3D12_LIBRARIES
      d3d12
      dxgi
      d3dcompiler
    )
  else()
    message(STATUS "Direct3D 12 backend disabled because the Windows SDK Direct3D headers were not found; native stub renderer remains available.")
  endif()
endif()

if(APPLE)
  list(APPEND ASTER_RENDER_SOURCES
    "${PROJECT_SOURCE_DIR}/src/render/render_device_metal.mm"
  )
elseif(ASTER_HAS_D3D12_BACKEND)
  list(APPEND ASTER_RENDER_SOURCES
    "${PROJECT_SOURCE_DIR}/src/render/render_device_d3d12.cpp"
  )
else()
  list(APPEND ASTER_RENDER_SOURCES
    "${PROJECT_SOURCE_DIR}/src/render/native_render_backend_stub.cpp"
  )
endif()

set(ASTER_GEOMETRY_SOURCES
  "${PROJECT_SOURCE_DIR}/src/geometry/architectural_mesh.cpp"
  "${PROJECT_SOURCE_DIR}/src/geometry/brush_level_mesh.cpp"
  "${PROJECT_SOURCE_DIR}/src/geometry/cable_mesh.cpp"
  "${PROJECT_SOURCE_DIR}/src/geometry/castle_course.cpp"
  "${PROJECT_SOURCE_DIR}/src/geometry/cave_system.cpp"
  "${PROJECT_SOURCE_DIR}/src/geometry/cave_web_mesh.cpp"
  "${PROJECT_SOURCE_DIR}/src/geometry/energy_conduit_mesh.cpp"
  "${PROJECT_SOURCE_DIR}/src/geometry/fracture_mesh.cpp"
  "${PROJECT_SOURCE_DIR}/src/geometry/generated_scenery.cpp"
  "${PROJECT_SOURCE_DIR}/src/geometry/geometry_operations.cpp"
  "${PROJECT_SOURCE_DIR}/src/geometry/implicit_surface.cpp"
  "${PROJECT_SOURCE_DIR}/src/geometry/mesh_cut.cpp"
  "${PROJECT_SOURCE_DIR}/src/geometry/mesh_authoring.cpp"
  "${PROJECT_SOURCE_DIR}/src/geometry/mesh_modeling.cpp"
  "${PROJECT_SOURCE_DIR}/src/geometry/mesh_projection.cpp"
  "${PROJECT_SOURCE_DIR}/src/geometry/nature_mesh.cpp"
  "${PROJECT_SOURCE_DIR}/src/geometry/primate_anatomy.cpp"
  "${PROJECT_SOURCE_DIR}/src/geometry/procedural_modeling.cpp"
  "${PROJECT_SOURCE_DIR}/src/geometry/stroke_mesh.cpp"
  "${PROJECT_SOURCE_DIR}/src/geometry/terrain_mesh.cpp"
  "${PROJECT_SOURCE_DIR}/src/geometry/terrain_sculpt.cpp"
  "${PROJECT_SOURCE_DIR}/src/geometry/tube_mesh.cpp"
  "${PROJECT_SOURCE_DIR}/src/geometry/traversable_manifold.cpp"
  "${PROJECT_SOURCE_DIR}/src/geometry/voxel_cave.cpp"
  "${PROJECT_SOURCE_DIR}/src/geometry/voxel_structure.cpp"
  "${PROJECT_SOURCE_DIR}/src/geometry/water_mesh.cpp"
)

set(ASTER_SCENE_SOURCES
  "${PROJECT_SOURCE_DIR}/src/scene/avatar.cpp"
  "${PROJECT_SOURCE_DIR}/src/scene/scene.cpp"
  "${PROJECT_SOURCE_DIR}/src/scene/scene_coherence.cpp"
  "${PROJECT_SOURCE_DIR}/src/scene/scene_trace.cpp"
)

set(ASTER_PHYSICS_SOURCES
  "${PROJECT_SOURCE_DIR}/src/physics/climb_locomotion.cpp"
  "${PROJECT_SOURCE_DIR}/src/physics/contact_query.cpp"
  "${PROJECT_SOURCE_DIR}/src/physics/fluid_locomotion.cpp"
  "${PROJECT_SOURCE_DIR}/src/physics/physics_world.cpp"
  "${PROJECT_SOURCE_DIR}/src/physics/placement_validation.cpp"
  "${PROJECT_SOURCE_DIR}/src/physics/surface_support.cpp"
  "${PROJECT_SOURCE_DIR}/src/physics/terrain_contact.cpp"
  "${PROJECT_SOURCE_DIR}/src/physics/xpbd_authoring.cpp"
  "${PROJECT_SOURCE_DIR}/src/physics/xpbd_constraints.cpp"
)

set(ASTER_SYSTEMS_SOURCES
  "${PROJECT_SOURCE_DIR}/src/systems/animation_system.cpp"
  "${PROJECT_SOURCE_DIR}/src/systems/classic_actor_runtime.cpp"
  "${PROJECT_SOURCE_DIR}/src/systems/creature_motion.cpp"
  "${PROJECT_SOURCE_DIR}/src/systems/equipment_system.cpp"
  "${PROJECT_SOURCE_DIR}/src/systems/interaction_system.cpp"
  "${PROJECT_SOURCE_DIR}/src/systems/inventory_system.cpp"
  "${PROJECT_SOURCE_DIR}/src/systems/item_system.cpp"
  "${PROJECT_SOURCE_DIR}/src/systems/light_system.cpp"
  "${PROJECT_SOURCE_DIR}/src/systems/mining_system.cpp"
  "${PROJECT_SOURCE_DIR}/src/systems/particle_system.cpp"
  "${PROJECT_SOURCE_DIR}/src/systems/player_motion.cpp"
  "${PROJECT_SOURCE_DIR}/src/systems/third_person_camera.cpp"
  "${PROJECT_SOURCE_DIR}/src/systems/world_mechanism.cpp"
)

set(ASTER_AI_SOURCES
  "${PROJECT_SOURCE_DIR}/src/ai/behavior_tree.cpp"
  "${PROJECT_SOURCE_DIR}/src/ai/steering.cpp"
)

set(ASTER_INPUT_SOURCES
  "${PROJECT_SOURCE_DIR}/src/input/control_scheme.cpp"
)

set(ASTER_UI_SOURCES
  "${PROJECT_SOURCE_DIR}/src/ui/automap_model.cpp"
  "${PROJECT_SOURCE_DIR}/src/ui/classic_hud_signals.cpp"
  "${PROJECT_SOURCE_DIR}/src/ui/control_legend.cpp"
  "${PROJECT_SOURCE_DIR}/src/ui/editor_ui.cpp"
  "${PROJECT_SOURCE_DIR}/src/ui/hud_layer.cpp"
  "${PROJECT_SOURCE_DIR}/src/ui/inventory_overlay.cpp"
  "${PROJECT_SOURCE_DIR}/src/ui/transition_wipe.cpp"
  "${PROJECT_SOURCE_DIR}/src/ui/ui_canvas.cpp"
)

set(ASTER_NET_SOURCES
  "${PROJECT_SOURCE_DIR}/src/net/lockstep_command_channel.cpp"
  "${PROJECT_SOURCE_DIR}/src/net/net_message.cpp"
  "${PROJECT_SOURCE_DIR}/src/net/node_router.cpp"
  "${PROJECT_SOURCE_DIR}/src/net/tcp_node.cpp"
)

set(ASTER_SAMPLES_SOURCES
  "${PROJECT_SOURCE_DIR}/src/samples/aster_grid_tactics/aster_grid_tactics.cpp"
  "${PROJECT_SOURCE_DIR}/src/samples/lumen_run/construction_yard.cpp"
  "${PROJECT_SOURCE_DIR}/src/samples/lumen_run/lumen_run.cpp"
  "${PROJECT_SOURCE_DIR}/src/samples/lumen_run/interaction.cpp"
  "${PROJECT_SOURCE_DIR}/src/samples/lumen_run/mining.cpp"
  "${PROJECT_SOURCE_DIR}/src/samples/lumen_run/scene.cpp"
  "${PROJECT_SOURCE_DIR}/src/samples/lumen_run/simulation.cpp"
  "${PROJECT_SOURCE_DIR}/src/samples/lumen_run/validation.cpp"
  "${PROJECT_SOURCE_DIR}/src/samples/showcase_scenes.cpp"
)

set(ASTER_PLATFORM_SOURCES)
set(ASTER_HAS_WAYLAND_BACKEND OFF)

if(APPLE)
  list(APPEND ASTER_PLATFORM_SOURCES "${PROJECT_SOURCE_DIR}/src/platform/window_macos.mm")
elseif(WIN32)
  list(APPEND ASTER_PLATFORM_SOURCES "${PROJECT_SOURCE_DIR}/src/platform/window_windows.cpp")
else()
  list(APPEND ASTER_PLATFORM_SOURCES "${PROJECT_SOURCE_DIR}/src/platform/window_linux.cpp")

  if(NOT ASTER_ENABLE_WAYLAND_MODE STREQUAL "OFF")
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
      pkg_check_modules(WAYLAND_CLIENT QUIET IMPORTED_TARGET wayland-client)
      pkg_check_modules(WAYLAND_PROTOCOLS QUIET wayland-protocols)
      if(WAYLAND_PROTOCOLS_FOUND)
        pkg_get_variable(WAYLAND_PROTOCOLS_PKGDATADIR wayland-protocols pkgdatadir)
      endif()
    endif()
    find_program(WAYLAND_SCANNER_EXECUTABLE wayland-scanner)
  endif()

  set(ASTER_WAYLAND_PROTOCOLS_AVAILABLE OFF)
  set(ASTER_WAYLAND_MISSING_REASON "pkg-config module wayland-client (Ubuntu: libwayland-dev), wayland-protocols, or wayland-scanner was not found")
  if(WAYLAND_CLIENT_FOUND AND WAYLAND_PROTOCOLS_FOUND AND WAYLAND_PROTOCOLS_PKGDATADIR AND
     WAYLAND_SCANNER_EXECUTABLE)
    set(ASTER_WAYLAND_XDG_SHELL_XML
      "${WAYLAND_PROTOCOLS_PKGDATADIR}/stable/xdg-shell/xdg-shell.xml")
    set(ASTER_WAYLAND_RELATIVE_POINTER_XML
      "${WAYLAND_PROTOCOLS_PKGDATADIR}/unstable/relative-pointer/relative-pointer-unstable-v1.xml")
    set(ASTER_WAYLAND_POINTER_CONSTRAINTS_XML
      "${WAYLAND_PROTOCOLS_PKGDATADIR}/unstable/pointer-constraints/pointer-constraints-unstable-v1.xml")
    set(ASTER_WAYLAND_PROTOCOLS_AVAILABLE ON)
    foreach(wayland_protocol_xml IN ITEMS
        "${ASTER_WAYLAND_XDG_SHELL_XML}"
        "${ASTER_WAYLAND_RELATIVE_POINTER_XML}"
        "${ASTER_WAYLAND_POINTER_CONSTRAINTS_XML}")
      if(NOT EXISTS "${wayland_protocol_xml}")
        set(ASTER_WAYLAND_PROTOCOLS_AVAILABLE OFF)
        set(ASTER_WAYLAND_MISSING_REASON "missing Wayland protocol XML: ${wayland_protocol_xml}")
      endif()
    endforeach()
  endif()

  if(ASTER_WAYLAND_PROTOCOLS_AVAILABLE)
    set(ASTER_HAS_WAYLAND_BACKEND ON)
    set(ASTER_WAYLAND_PROTOCOL_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated/wayland")
    file(MAKE_DIRECTORY "${ASTER_WAYLAND_PROTOCOL_DIR}")

    function(aster_generate_wayland_protocol protocol_name protocol_xml)
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
      list(APPEND ASTER_PLATFORM_SOURCES "${protocol_code}" "${protocol_header}")
      set(ASTER_PLATFORM_SOURCES "${ASTER_PLATFORM_SOURCES}" PARENT_SCOPE)
    endfunction()

    aster_generate_wayland_protocol(
      xdg-shell
      "${ASTER_WAYLAND_XDG_SHELL_XML}"
    )
    aster_generate_wayland_protocol(
      relative-pointer-unstable-v1
      "${ASTER_WAYLAND_RELATIVE_POINTER_XML}"
    )
    aster_generate_wayland_protocol(
      pointer-constraints-unstable-v1
      "${ASTER_WAYLAND_POINTER_CONSTRAINTS_XML}"
    )
  elseif(ASTER_ENABLE_WAYLAND_MODE STREQUAL "ON")
    message(FATAL_ERROR "Wayland backend requested but ${ASTER_WAYLAND_MISSING_REASON}.")
  elseif(ASTER_ENABLE_WAYLAND_MODE STREQUAL "AUTO")
    message(STATUS "Wayland backend disabled (${ASTER_WAYLAND_MISSING_REASON}); raw X11 fallback remains available.")
  endif()
endif()

set(ASTER_ENGINE_COMPILE_DEFINITIONS
  ASTER_HAS_PROFILING_RUNTIME=$<IF:$<BOOL:${ASTER_ENABLE_PROFILING}>,1,0>
  ASTER_HAS_NETWORK_TRANSPORT=$<IF:$<BOOL:${ASTER_BUILD_NETWORK}>,1,0>
  ASTER_HAS_WAYLAND_BACKEND=$<IF:$<BOOL:${ASTER_HAS_WAYLAND_BACKEND}>,1,0>
  ASTER_HAS_D3D12_BACKEND=$<IF:$<BOOL:${ASTER_HAS_D3D12_BACKEND}>,1,0>
)

function(aster_configure_engine_compile_target target_name)
  target_include_directories(${target_name} PUBLIC
    ${PROJECT_SOURCE_DIR}/include
  )
  if(UNIX AND NOT APPLE AND ASTER_HAS_WAYLAND_BACKEND)
    target_include_directories(${target_name} PRIVATE ${ASTER_WAYLAND_PROTOCOL_DIR})
    target_link_libraries(${target_name} PRIVATE PkgConfig::WAYLAND_CLIENT)
  endif()
  target_compile_definitions(${target_name} PUBLIC ${ASTER_ENGINE_COMPILE_DEFINITIONS})
  if(MSVC)
    target_compile_options(${target_name} PRIVATE /W4 /permissive-)
  else()
    target_compile_options(${target_name} PRIVATE -Wall -Wextra -Wpedantic)
  endif()
endfunction()

set(ASTER_ENGINE_OBJECT_TARGETS)
function(aster_add_engine_object_library target_name)
  add_library(${target_name} OBJECT ${ARGN})
  set_target_properties(${target_name} PROPERTIES POSITION_INDEPENDENT_CODE ON)
  aster_configure_engine_compile_target(${target_name})
  list(APPEND ASTER_ENGINE_OBJECT_TARGETS ${target_name})
  set(ASTER_ENGINE_OBJECT_TARGETS "${ASTER_ENGINE_OBJECT_TARGETS}" PARENT_SCOPE)
endfunction()

aster_add_engine_object_library(aster_core_objects ${ASTER_CORE_SOURCES})
aster_add_engine_object_library(aster_learning_objects ${ASTER_LEARNING_SOURCES})
aster_add_engine_object_library(aster_rhi_objects ${ASTER_RHI_SOURCES})
aster_add_engine_object_library(aster_framegraph_objects ${ASTER_FRAMEGRAPH_SOURCES})
aster_add_engine_object_library(aster_graphics_core7_objects ${ASTER_GRAPHICS_CORE7_SOURCES})
aster_add_engine_object_library(aster_asset_objects ${ASTER_ASSET_SOURCES})
aster_add_engine_object_library(aster_material_objects ${ASTER_MATERIAL_SOURCES})
aster_add_engine_object_library(aster_texture_objects ${ASTER_TEXTURE_SOURCES})
aster_add_engine_object_library(aster_shader_objects ${ASTER_SHADER_SOURCES})
aster_add_engine_object_library(aster_gameplay_objects ${ASTER_GAMEPLAY_SOURCES})
aster_add_engine_object_library(aster_render_objects ${ASTER_RENDER_SOURCES})
aster_add_engine_object_library(aster_geometry_objects ${ASTER_GEOMETRY_SOURCES})
aster_add_engine_object_library(aster_scene_objects ${ASTER_SCENE_SOURCES})
aster_add_engine_object_library(aster_physics_objects ${ASTER_PHYSICS_SOURCES})
aster_add_engine_object_library(aster_systems_objects ${ASTER_SYSTEMS_SOURCES})
aster_add_engine_object_library(aster_ai_objects ${ASTER_AI_SOURCES})
aster_add_engine_object_library(aster_input_objects ${ASTER_INPUT_SOURCES})
aster_add_engine_object_library(aster_ui_objects ${ASTER_UI_SOURCES})
aster_add_engine_object_library(aster_net_objects ${ASTER_NET_SOURCES})
aster_add_engine_object_library(aster_samples_objects ${ASTER_SAMPLES_SOURCES})
aster_add_engine_object_library(aster_platform_objects ${ASTER_PLATFORM_SOURCES})

add_library(aster_game_sdk STATIC ${ASTER_GAME_SDK_SOURCES})
add_library(aster::game_sdk ALIAS aster_game_sdk)
target_compile_features(aster_game_sdk PUBLIC cxx_std_20)
target_include_directories(aster_game_sdk PUBLIC
  $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
set_target_properties(aster_game_sdk PROPERTIES EXPORT_NAME game_sdk)
set_target_properties(aster_game_sdk PROPERTIES POSITION_INDEPENDENT_CODE ON)
if(MSVC)
  target_compile_options(aster_game_sdk PRIVATE /W4 /permissive-)
else()
  target_compile_options(aster_game_sdk PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_library(aster_engine_internal STATIC)
add_library(aster ALIAS aster_engine_internal)
foreach(engine_object_target IN LISTS ASTER_ENGINE_OBJECT_TARGETS)
  target_sources(aster_engine_internal PRIVATE $<TARGET_OBJECTS:${engine_object_target}>)
endforeach()
add_dependencies(aster_engine_internal aster_rust_runtime ${ASTER_ENGINE_OBJECT_TARGETS})
set_target_properties(aster_engine_internal PROPERTIES
  POSITION_INDEPENDENT_CODE ON
  CXX_VISIBILITY_PRESET hidden
  VISIBILITY_INLINES_HIDDEN YES
)

target_include_directories(aster_engine_internal PUBLIC
  ${PROJECT_SOURCE_DIR}/include
)
if(UNIX AND NOT APPLE AND ASTER_HAS_WAYLAND_BACKEND)
  target_include_directories(aster_engine_internal PRIVATE ${ASTER_WAYLAND_PROTOCOL_DIR})
endif()

target_link_libraries(aster_engine_internal PUBLIC Threads::Threads aster_runtime)
if(UNIX AND NOT APPLE AND ASTER_HAS_WAYLAND_BACKEND)
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
  if(ASTER_HAS_D3D12_BACKEND)
    target_link_libraries(aster_engine_internal PUBLIC ${ASTER_D3D12_LIBRARIES})
  endif()
endif()

target_compile_definitions(aster_engine_internal PUBLIC
  ${ASTER_ENGINE_COMPILE_DEFINITIONS}
)

add_library(aster_kernel SHARED src/kernel/kernel.cpp)
add_library(aster::kernel ALIAS aster_kernel)
target_compile_features(aster_kernel PUBLIC cxx_std_20)
target_include_directories(aster_kernel PUBLIC
  $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
target_compile_definitions(aster_kernel PRIVATE ASTER_KERNEL_BUILD=1)
target_link_libraries(aster_kernel PRIVATE aster_engine_internal aster_game_sdk)
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
install(TARGETS aster_game_sdk
  EXPORT AsterGameSdkTargets
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)
install(DIRECTORY include/aster/kernel DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/aster)
install(DIRECTORY include/aster/game_sdk DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/aster)
install(DIRECTORY include/aster/math DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/aster)
install(FILES abi/aster_kernel.symbols DESTINATION ${CMAKE_INSTALL_DATADIR}/aster/abi)
configure_package_config_file(
  "${PROJECT_SOURCE_DIR}/cmake/AsterKernelConfig.cmake.in"
  "${PROJECT_BINARY_DIR}/AsterKernelConfig.cmake"
  INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/AsterKernel
)
configure_package_config_file(
  "${PROJECT_SOURCE_DIR}/cmake/AsterGameSdkConfig.cmake.in"
  "${PROJECT_BINARY_DIR}/AsterGameSdkConfig.cmake"
  INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/AsterGameSdk
)
write_basic_package_version_file(
  "${PROJECT_BINARY_DIR}/AsterKernelConfigVersion.cmake"
  VERSION ${ASTER_KERNEL_PACKAGE_VERSION}
  COMPATIBILITY SameMajorVersion
)
write_basic_package_version_file(
  "${PROJECT_BINARY_DIR}/AsterGameSdkConfigVersion.cmake"
  VERSION ${PROJECT_VERSION}
  COMPATIBILITY SameMajorVersion
)
install(EXPORT AsterKernelTargets
  NAMESPACE aster::
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/AsterKernel
)
install(FILES
  "${PROJECT_BINARY_DIR}/AsterKernelConfig.cmake"
  "${PROJECT_BINARY_DIR}/AsterKernelConfigVersion.cmake"
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/AsterKernel
)
install(EXPORT AsterGameSdkTargets
  NAMESPACE aster::
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/AsterGameSdk
)
install(FILES
  "${PROJECT_BINARY_DIR}/AsterGameSdkConfig.cmake"
  "${PROJECT_BINARY_DIR}/AsterGameSdkConfigVersion.cmake"
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/AsterGameSdk
)

if(ASTER_BUILD_STUDIO)
  add_executable(aster_studio apps/studio/main.cpp)
  target_link_libraries(aster_studio PRIVATE aster_engine_internal)

  add_executable(aster_lumen_run apps/samples/lumen_run/main.cpp)
  target_link_libraries(aster_lumen_run PRIVATE aster_engine_internal aster_game_sdk)
  target_compile_definitions(aster_lumen_run PRIVATE
    ASTER_LUMEN_RUN_PROJECT="${PROJECT_SOURCE_DIR}/projects/lumen_run/lumen_run.asterproj"
  )

  add_executable(aster_grid_tactics apps/samples/aster_grid_tactics/main.cpp)
  target_link_libraries(aster_grid_tactics PRIVATE aster_engine_internal aster_game_sdk)
  target_compile_definitions(aster_grid_tactics PRIVATE
    ASTER_GRID_TACTICS_PROJECT="${PROJECT_SOURCE_DIR}/projects/aster_grid_tactics/aster_grid_tactics.asterproj"
  )
endif()

if(ASTER_BUILD_PREVIEW)
  add_executable(aster_quickstart apps/examples/quickstart.cpp)
  target_link_libraries(aster_quickstart PRIVATE aster_engine_internal)
  target_compile_definitions(aster_quickstart PRIVATE
    ASTER_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
  )

  add_executable(aster_preview apps/offline/preview_main.cpp)
  target_link_libraries(aster_preview PRIVATE aster_engine_internal)

  add_executable(aster_material_lab apps/material_lab/main.cpp)
  target_link_libraries(aster_material_lab PRIVATE aster_engine_internal)
endif()

if(ASTER_BUILD_NETWORK)
  add_executable(aster_net_probe apps/net/probe_main.cpp)
  target_link_libraries(aster_net_probe PRIVATE aster_engine_internal)
endif()

if(ASTER_BUILD_TESTS)
  enable_testing()
  function(aster_configure_test_target test_name)
    if(MSVC)
      target_compile_options(${test_name} PRIVATE /UNDEBUG)
    else()
      target_compile_options(${test_name} PRIVATE -UNDEBUG)
    endif()
  endfunction()

  function(aster_add_test test_name test_source)
    add_executable(${test_name} ${test_source})
    target_link_libraries(${test_name} PRIVATE aster_engine_internal)
    target_include_directories(${test_name} PRIVATE ${PROJECT_SOURCE_DIR}/tests)
    aster_configure_test_target(${test_name})
    add_test(NAME ${test_name} COMMAND ${test_name})
  endfunction()

  aster_add_test(aster_core_tests tests/core_tests.cpp)
  aster_add_test(aster_math_tests tests/math_tests.cpp)
  aster_add_test(aster_math_property_tests tests/math_property_tests.cpp)
  aster_add_test(aster_geometry_tests tests/geometry_tests.cpp)
  add_executable(aster_cercopithecidae_modeling_tests
    tests/cercopithecidae_modeling_tests.cpp)
  target_link_libraries(aster_cercopithecidae_modeling_tests PRIVATE aster_engine_internal)
  target_include_directories(aster_cercopithecidae_modeling_tests PRIVATE
    ${PROJECT_SOURCE_DIR}/tests)
  aster_configure_test_target(aster_cercopithecidae_modeling_tests)
  target_compile_definitions(aster_cercopithecidae_modeling_tests PRIVATE
    ASTER_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
    ASTER_ASSETC_EXECUTABLE="${ASTER_ASSETC_EXECUTABLE}"
  )
  add_dependencies(aster_cercopithecidae_modeling_tests aster_rust_runtime)
  add_test(NAME aster_cercopithecidae_modeling_tests
           COMMAND aster_cercopithecidae_modeling_tests)
  add_executable(aster_pipe_runtime_asset_tests tests/pipe_runtime_asset_tests.cpp)
  target_link_libraries(aster_pipe_runtime_asset_tests PRIVATE aster_engine_internal)
  target_include_directories(aster_pipe_runtime_asset_tests PRIVATE ${PROJECT_SOURCE_DIR}/tests)
  aster_configure_test_target(aster_pipe_runtime_asset_tests)
  target_compile_definitions(aster_pipe_runtime_asset_tests PRIVATE
    ASTER_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
    ASTER_ASSETC_EXECUTABLE="${ASTER_ASSETC_EXECUTABLE}"
  )
  add_dependencies(aster_pipe_runtime_asset_tests aster_rust_runtime)
  add_test(NAME aster_pipe_runtime_asset_tests COMMAND aster_pipe_runtime_asset_tests)
  add_executable(aster_asset_factory_tests tests/asset_factory_tests.cpp)
  target_link_libraries(aster_asset_factory_tests PRIVATE aster_engine_internal)
  target_include_directories(aster_asset_factory_tests PRIVATE ${PROJECT_SOURCE_DIR}/tests)
  aster_configure_test_target(aster_asset_factory_tests)
  target_compile_definitions(aster_asset_factory_tests PRIVATE
    ASTER_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
    ASTER_ASSETC_EXECUTABLE="${ASTER_ASSETC_EXECUTABLE}"
  )
  add_dependencies(aster_asset_factory_tests aster_rust_runtime)
  add_test(NAME aster_asset_factory_tests COMMAND aster_asset_factory_tests)
  add_executable(aster_asset_cache_tests tests/asset_cache_tests.cpp)
  target_link_libraries(aster_asset_cache_tests PRIVATE aster_engine_internal)
  target_include_directories(aster_asset_cache_tests PRIVATE ${PROJECT_SOURCE_DIR}/tests)
  aster_configure_test_target(aster_asset_cache_tests)
  target_compile_definitions(aster_asset_cache_tests PRIVATE
    ASTER_ASSETC_EXECUTABLE="${ASTER_ASSETC_EXECUTABLE}"
  )
  add_dependencies(aster_asset_cache_tests aster_rust_runtime)
  add_test(NAME aster_asset_cache_tests COMMAND aster_asset_cache_tests)
  add_executable(aster_asset_tooling_port_tests tests/asset_tooling_port_tests.cpp)
  target_link_libraries(aster_asset_tooling_port_tests PRIVATE aster_engine_internal)
  target_include_directories(aster_asset_tooling_port_tests PRIVATE ${PROJECT_SOURCE_DIR}/tests)
  aster_configure_test_target(aster_asset_tooling_port_tests)
  target_compile_definitions(aster_asset_tooling_port_tests PRIVATE
    ASTER_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
  )
  add_test(NAME aster_asset_tooling_port_tests COMMAND aster_asset_tooling_port_tests)
  add_executable(aster_engine_contract_port_tests tests/engine_contract_port_tests.cpp)
  target_link_libraries(aster_engine_contract_port_tests PRIVATE aster_engine_internal)
  target_include_directories(aster_engine_contract_port_tests PRIVATE ${PROJECT_SOURCE_DIR}/tests)
  aster_configure_test_target(aster_engine_contract_port_tests)
  target_compile_definitions(aster_engine_contract_port_tests PRIVATE
    ASTER_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
  )
  add_test(NAME aster_engine_contract_port_tests COMMAND aster_engine_contract_port_tests)
  foreach(engine_contract_case
      asset_path_index_registry_and_queue
      derived_asset_cache_policy_and_rollup
      content_pack_overlay
      job_graph_trace
      artifact_writer
  )
    add_test(NAME aster_engine_contract_port_tests.${engine_contract_case}
             COMMAND aster_engine_contract_port_tests ${engine_contract_case})
  endforeach()
  add_executable(aster_kernel_public_consumer tests/kernel_public_consumer.cpp)
  target_link_libraries(aster_kernel_public_consumer PRIVATE aster_kernel)
  aster_configure_test_target(aster_kernel_public_consumer)
  add_test(NAME aster_kernel_public_consumer COMMAND aster_kernel_public_consumer)

  add_executable(aster_game_sdk_public_consumer tests/game_sdk_public_consumer.cpp)
  target_link_libraries(aster_game_sdk_public_consumer PRIVATE aster_game_sdk)
  aster_configure_test_target(aster_game_sdk_public_consumer)
  target_compile_definitions(aster_game_sdk_public_consumer PRIVATE
    ASTER_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
  )
  add_test(NAME aster_game_sdk_public_consumer COMMAND aster_game_sdk_public_consumer)

  add_executable(aster_learning_runtime_tests tests/learning_runtime_tests.cpp)
  target_link_libraries(aster_learning_runtime_tests PRIVATE aster_engine_internal)
  target_include_directories(aster_learning_runtime_tests PRIVATE ${PROJECT_SOURCE_DIR}/tests)
  aster_configure_test_target(aster_learning_runtime_tests)
  foreach(learning_runtime_case
      good_lumen_trace
      missing_tool_evidence
      unsupported_scaffold_rationale
      false_mastery_rejected
      missing_workflow_evaluation
      memory_controller_reducer
      memory_controller_provider_block
  )
    add_test(NAME aster_learning_runtime.${learning_runtime_case}
             COMMAND aster_learning_runtime_tests ${learning_runtime_case})
  endforeach()

  if(ASTER_BUILD_STUDIO)
    add_test(NAME aster_lumen_run_validate_cave
             COMMAND aster_lumen_run --validate-cave)
    add_test(NAME aster_lumen_run_cave_vision
             COMMAND aster_lumen_run --playtest-vision --vision-route cave-regression
                     --vision-out "${PROJECT_BINARY_DIR}/test-artifacts/lumen_run_cave_vision"
                     --vision-width 480 --vision-height 270 --no-vsync)
    add_test(NAME aster_lumen_run_cave_lighting_vision
             COMMAND aster_lumen_run --playtest-lighting --lighting-route cave-regression
                     --lighting-out "${PROJECT_BINARY_DIR}/test-artifacts/lumen_run_cave_lighting"
                     --lighting-width 480 --lighting-height 270 --no-vsync)
    add_test(NAME aster_lumen_run_smoke_test
             COMMAND aster_lumen_run --smoke-test --no-vsync)
    add_test(NAME aster_studio_material_lab_smoke
             COMMAND aster_studio --smoke-test --asset-db
                     "${PROJECT_SOURCE_DIR}/showcases/material_lab/cooked/desktop/assetdb.asterdb.json"
                     --window-width 640 --window-height 480)
  endif()

  add_executable(aster_kernel_contract_tests tests/kernel_contract_tests.cpp)
  target_link_libraries(aster_kernel_contract_tests PRIVATE aster_kernel)
  aster_configure_test_target(aster_kernel_contract_tests)
  target_compile_definitions(aster_kernel_contract_tests PRIVATE
    ASTER_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
  )
  add_test(NAME aster_kernel_contract_tests COMMAND aster_kernel_contract_tests)

  add_test(NAME aster_external_app_minimal_install_tree
    COMMAND ${CMAKE_COMMAND}
      -DASTER_SOURCE_DIR=${PROJECT_SOURCE_DIR}
      -DASTER_BINARY_DIR=${PROJECT_BINARY_DIR}
      -DASTER_INSTALL_DIR=${PROJECT_BINARY_DIR}/external_app_minimal_install
      -DASTER_EXTERNAL_BUILD_DIR=${PROJECT_BINARY_DIR}/external_app_minimal_build
      -DASTER_CONFIG=$<CONFIG>
      -P ${PROJECT_SOURCE_DIR}/tools/check_external_app_minimal.cmake
  )

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
  aster_configure_test_target(aster_render_scene_tests)
  target_compile_definitions(aster_render_scene_tests PRIVATE
    ASTER_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
  )
  foreach(render_scene_case
      framebuffer_origin
      ui_text_fitting
      hud_visibility
      scene_contract
      industrial_pipe_scene
      showcase_lab_scenes
      showcase_lab_scenes_environment_radiance
      showcase_lab_scenes_debug_view_response
      showcase_lab_scenes_contact_shadow_falloff
      software_preview_renderer
      software_lighting_probe_media
      software_depth_policy_coplanar
      software_depth_policy_object_order
      prepare_scene_custom_mesh_cache
      frame_math_diagnostics
      strict_perceptual_renderable_coverage
      warn_perceptual_renderable_coverage
      renderer_backend_perceptual_truth_gap_for_accepted_primitive
      frame_debugger_material_binding_trace
      frame_debugger_perception_ledger_trace
      frame_debugger_asset_provenance_trace
      frame_debugger_procedural_asset_graph_trace
      frame_debugger_evidence_timeline_and_regression_lab
      software_reference_frame_resource_captures
      graphics_core7_strict_rejects_missing_native_truth
      graphics_core7_software_reference_truth_accepted
      visual_regression_artifacts
      retro_style_neutral_preview
      retro_style_preview_effects
      retro_style_emissive_gain
      material_render_policies
      render_extraction_and_cache_contracts
      runtime_light_policy
      simple_draw_api
      rhi_resource_registry
      rhi_resource_lifetime_validator
      rhi_explicit_gpu_contracts
      frame_graph
      backend_kinds
      material_compiler
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

  add_executable(aster_render_backend_conformance_tests
    tests/render_backend_conformance_tests.cpp)
  target_link_libraries(aster_render_backend_conformance_tests PRIVATE aster_engine_internal)
  target_include_directories(aster_render_backend_conformance_tests PRIVATE
    ${PROJECT_SOURCE_DIR}/tests)
  aster_configure_test_target(aster_render_backend_conformance_tests)
  target_compile_definitions(aster_render_backend_conformance_tests PRIVATE
    ASTER_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
  )
  foreach(render_backend_conformance_case
      software_deterministic_hash_and_forensics
      native_backend_conforms_when_available
      backend_capability_table_contracts
      backend_visual_truth_gap_is_reported
      golden_lab_scenes
      cave_conformance_software_golden
      resource_capability_mismatch_requires_resource_mask
      backend_feature_certification_rejects_lying_null
      graphics_core7_strict_rejects_missing_native_proof
      graphics_core7_advertised_native_proof_signals
      math_contract_certification_rejects_projection_drift
      native_cave_conformance_when_available
      d3d12_swapchain_presentation_when_available
      native_material_pipeline_key_tracks_feature_bits
      native_lab_scenes_match_software_reference_when_available
      failure_artifact_writer
  )
    add_test(NAME aster_render_backend_conformance_tests.${render_backend_conformance_case}
             COMMAND aster_render_backend_conformance_tests ${render_backend_conformance_case})
  endforeach()

  add_executable(aster_material_shader_system_tests tests/material_shader_system_tests.cpp)
  target_link_libraries(aster_material_shader_system_tests PRIVATE aster_engine_internal)
  target_include_directories(aster_material_shader_system_tests PRIVATE ${PROJECT_SOURCE_DIR}/tests)
  aster_configure_test_target(aster_material_shader_system_tests)
  target_compile_definitions(aster_material_shader_system_tests PRIVATE
    ASTER_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
  )
  foreach(material_shader_case
      material_asset_parser_and_compiler
      biological_integument_material_aliases
      material_asset_metadata_round_trip
      material_authoring_graph_and_lab_audit
      material_lab_preview_debug_views
      shader_library_and_reflection
      texture_validation_and_debug
      render_quality_profile
      render_style_profile
      native_render_style_shaders
      shader_matrix_layout_and_normal_map_contracts
      hot_reload_snapshot
      invalid_material_diagnostics
      procedural_asset_graph_package
  )
    add_test(NAME aster_material_shader_system_tests.${material_shader_case}
             COMMAND aster_material_shader_system_tests ${material_shader_case})
  endforeach()
  aster_add_test(aster_systems_tests tests/systems_tests.cpp)
  aster_add_test(aster_physics_tests tests/physics_tests.cpp)
  aster_add_test(aster_lumen_run_tests tests/samples/lumen_run/lumen_run_tests.cpp)
  aster_add_test(aster_grid_tactics_tests
                 tests/samples/aster_grid_tactics/aster_grid_tactics_tests.cpp)
  foreach(grid_tactics_case
      scripted_run_reaches_victory
      deterministic_replay_hashes_match
      lockstep_command_channel_replays_script
      scene_contains_gameplay_signals_and_renders
      debug_grid_is_opt_in
      hud_uses_player_facing_language
      reset_restores_playable_state
  )
    add_test(NAME aster_grid_tactics_tests.${grid_tactics_case}
             COMMAND aster_grid_tactics_tests ${grid_tactics_case})
  endforeach()
  if(ASTER_BUILD_STUDIO)
    add_test(NAME aster_grid_tactics_smoke_test
             COMMAND aster_grid_tactics --smoke-test --no-vsync)
    add_test(NAME aster_grid_tactics_replay_self_test
             COMMAND aster_grid_tactics --replay-self-test --seed 4242 --ticks 260)
    add_test(NAME aster_grid_tactics_capture_test
             COMMAND aster_grid_tactics --screenshot
                     "${PROJECT_BINARY_DIR}/test-artifacts/aster_grid_tactics/grid_tactics.ppm"
                     --screenshot-frame 16 --no-vsync)
    add_test(NAME aster_grid_tactics_agent_plan
      COMMAND "${CMAKE_COMMAND}" -E env ${ASTER_CARGO_ENV}
              "${CARGO_EXECUTABLE}" run -p aster_assetc --bin aster_assetc --
              agent-plan --project projects/aster_grid_tactics/aster_grid_tactics.asterproj
              --output-schema
    )
    set_tests_properties(aster_grid_tactics_agent_plan PROPERTIES
      WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
    )
    add_test(NAME aster_grid_tactics_cook
      COMMAND "${CMAKE_COMMAND}" -E env ${ASTER_CARGO_ENV}
              "${CARGO_EXECUTABLE}" run -p aster_assetc --bin aster_assetc --
              cook --project projects/aster_grid_tactics/aster_grid_tactics.asterproj
              --platform desktop
              --output "${PROJECT_BINARY_DIR}/test-artifacts/aster_grid_tactics/cooked"
    )
    set_tests_properties(aster_grid_tactics_cook PROPERTIES
      WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
    )
  endif()
  if(ASTER_BUILD_NETWORK)
    aster_add_test(aster_network_tests tests/network_tests.cpp)
    add_test(NAME aster_net_probe COMMAND aster_net_probe)
  endif()
  add_test(NAME aster_rust_tests
    COMMAND "${CMAKE_COMMAND}" -E env ${ASTER_CARGO_ENV}
            "${CARGO_EXECUTABLE}" test --workspace ${ASTER_CARGO_TARGET_ARGS}
  )
  set_tests_properties(aster_rust_tests PROPERTIES
    WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
  )
endif()
