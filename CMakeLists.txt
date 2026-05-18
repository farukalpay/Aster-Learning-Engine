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
  set(ASTER_CARGO_PROFILE "debug")
  set(ASTER_RUST_LIBRARY "${PROJECT_SOURCE_DIR}/target/debug/${ASTER_RUST_STATICLIB_NAME}")
else()
  set(ASTER_CARGO_FLAGS "--release")
  set(ASTER_CARGO_PROFILE "release")
  set(ASTER_RUST_LIBRARY "${PROJECT_SOURCE_DIR}/target/release/${ASTER_RUST_STATICLIB_NAME}")
endif()
if(WIN32)
  set(ASTER_ASSETC_EXECUTABLE "${PROJECT_SOURCE_DIR}/target/${ASTER_CARGO_PROFILE}/aster_assetc.exe")
else()
  set(ASTER_ASSETC_EXECUTABLE "${PROJECT_SOURCE_DIR}/target/${ASTER_CARGO_PROFILE}/aster_assetc")
endif()

add_custom_command(
  OUTPUT "${ASTER_RUST_LIBRARY}"
  COMMAND "${CARGO_EXECUTABLE}" build --workspace ${ASTER_CARGO_FLAGS}
  WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
  DEPENDS
    "${PROJECT_SOURCE_DIR}/Cargo.toml"
    "${PROJECT_SOURCE_DIR}/crates/aster_content/Cargo.toml"
    "${PROJECT_SOURCE_DIR}/crates/aster_content/src/lib.rs"
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

set(ASTER_GAME_SDK_SOURCES
  "${PROJECT_SOURCE_DIR}/src/game_sdk/game_sdk.cpp"
)

set(ASTER_CORE_SOURCES
  "${PROJECT_SOURCE_DIR}/src/core/clock.cpp"
  "${PROJECT_SOURCE_DIR}/src/core/fixed_timestep.cpp"
  "${PROJECT_SOURCE_DIR}/src/core/frame_time_stats.cpp"
  "${PROJECT_SOURCE_DIR}/src/core/profiler.cpp"
  "${PROJECT_SOURCE_DIR}/src/core/work_budget.cpp"
  "${PROJECT_SOURCE_DIR}/src/math/transform.cpp"
)

set(ASTER_RHI_SOURCES
  "${PROJECT_SOURCE_DIR}/src/rhi/resource_registry.cpp"
)

set(ASTER_FRAMEGRAPH_SOURCES
  "${PROJECT_SOURCE_DIR}/src/framegraph/frame_graph.cpp"
  "${PROJECT_SOURCE_DIR}/src/framegraph/graph_compiler.cpp"
  "${PROJECT_SOURCE_DIR}/src/framegraph/transient_resource_allocator.cpp"
)

set(ASTER_ASSET_SOURCES
  "${PROJECT_SOURCE_DIR}/src/asset/compiled_scene_asset.cpp"
  "${PROJECT_SOURCE_DIR}/src/asset/mesh_pipeline.cpp"
  "${PROJECT_SOURCE_DIR}/src/asset/scene_asset_importer.cpp"
)

set(ASTER_MATERIAL_SOURCES
  "${PROJECT_SOURCE_DIR}/src/material/material_asset.cpp"
  "${PROJECT_SOURCE_DIR}/src/material/material_compiler.cpp"
  "${PROJECT_SOURCE_DIR}/src/material/material_graph.cpp"
  "${PROJECT_SOURCE_DIR}/src/material/material_instance.cpp"
  "${PROJECT_SOURCE_DIR}/src/material/material_parameter_block.cpp"
)

set(ASTER_TEXTURE_SOURCES
  "${PROJECT_SOURCE_DIR}/src/texture/texture_asset.cpp"
  "${PROJECT_SOURCE_DIR}/src/texture/texture_atlas.cpp"
  "${PROJECT_SOURCE_DIR}/src/texture/texture_compressor.cpp"
  "${PROJECT_SOURCE_DIR}/src/texture/texture_debug.cpp"
  "${PROJECT_SOURCE_DIR}/src/texture/texture_importer.cpp"
  "${PROJECT_SOURCE_DIR}/src/texture/texture_streamer.cpp"
)

set(ASTER_SHADER_SOURCES
  "${PROJECT_SOURCE_DIR}/src/shader/shader_compiler.cpp"
  "${PROJECT_SOURCE_DIR}/src/shader/shader_hot_reload.cpp"
  "${PROJECT_SOURCE_DIR}/src/shader/shader_library.cpp"
  "${PROJECT_SOURCE_DIR}/src/shader/shader_reflection.cpp"
  "${PROJECT_SOURCE_DIR}/src/shader/shader_variant.cpp"
)

set(ASTER_RENDER_SOURCES
  "${PROJECT_SOURCE_DIR}/src/render/camera.cpp"
  "${PROJECT_SOURCE_DIR}/src/render/frame_capture.cpp"
  "${PROJECT_SOURCE_DIR}/src/render/material_compiler.cpp"
  "${PROJECT_SOURCE_DIR}/src/render/mesh.cpp"
  "${PROJECT_SOURCE_DIR}/src/render/native_render_backend_null.cpp"
  "${PROJECT_SOURCE_DIR}/src/render/render_backend_common.cpp"
  "${PROJECT_SOURCE_DIR}/src/render/render_device.cpp"
  "${PROJECT_SOURCE_DIR}/src/render/render_graph.cpp"
  "${PROJECT_SOURCE_DIR}/src/render/render_graph_executor.cpp"
  "${PROJECT_SOURCE_DIR}/src/render/render_quality.cpp"
  "${PROJECT_SOURCE_DIR}/src/render/render_scene.cpp"
  "${PROJECT_SOURCE_DIR}/src/render/software_framebuffer.cpp"
  "${PROJECT_SOURCE_DIR}/src/render/software_preview_renderer.cpp"
)

if(APPLE)
  list(APPEND ASTER_RENDER_SOURCES
    "${PROJECT_SOURCE_DIR}/src/render/render_device_metal.mm"
  )
elseif(WIN32)
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
  "${PROJECT_SOURCE_DIR}/src/geometry/implicit_surface.cpp"
  "${PROJECT_SOURCE_DIR}/src/geometry/mesh_cut.cpp"
  "${PROJECT_SOURCE_DIR}/src/geometry/mesh_modeling.cpp"
  "${PROJECT_SOURCE_DIR}/src/geometry/mesh_projection.cpp"
  "${PROJECT_SOURCE_DIR}/src/geometry/nature_mesh.cpp"
  "${PROJECT_SOURCE_DIR}/src/geometry/stroke_mesh.cpp"
  "${PROJECT_SOURCE_DIR}/src/geometry/terrain_mesh.cpp"
  "${PROJECT_SOURCE_DIR}/src/geometry/terrain_sculpt.cpp"
  "${PROJECT_SOURCE_DIR}/src/geometry/tube_mesh.cpp"
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
)

set(ASTER_SYSTEMS_SOURCES
  "${PROJECT_SOURCE_DIR}/src/systems/animation_system.cpp"
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
)

set(ASTER_AI_SOURCES
  "${PROJECT_SOURCE_DIR}/src/ai/behavior_tree.cpp"
  "${PROJECT_SOURCE_DIR}/src/ai/steering.cpp"
)

set(ASTER_INPUT_SOURCES
  "${PROJECT_SOURCE_DIR}/src/input/control_scheme.cpp"
)

set(ASTER_UI_SOURCES
  "${PROJECT_SOURCE_DIR}/src/ui/control_legend.cpp"
  "${PROJECT_SOURCE_DIR}/src/ui/editor_ui.cpp"
  "${PROJECT_SOURCE_DIR}/src/ui/hud_layer.cpp"
  "${PROJECT_SOURCE_DIR}/src/ui/inventory_overlay.cpp"
  "${PROJECT_SOURCE_DIR}/src/ui/ui_canvas.cpp"
)

set(ASTER_NET_SOURCES
  "${PROJECT_SOURCE_DIR}/src/net/net_message.cpp"
  "${PROJECT_SOURCE_DIR}/src/net/node_router.cpp"
  "${PROJECT_SOURCE_DIR}/src/net/tcp_node.cpp"
)

set(ASTER_SAMPLES_SOURCES
  "${PROJECT_SOURCE_DIR}/src/samples/lumen_run.cpp"
  "${PROJECT_SOURCE_DIR}/src/samples/lumen_run_interaction.cpp"
  "${PROJECT_SOURCE_DIR}/src/samples/lumen_run_mining.cpp"
  "${PROJECT_SOURCE_DIR}/src/samples/lumen_run_scene.cpp"
  "${PROJECT_SOURCE_DIR}/src/samples/lumen_run_simulation.cpp"
  "${PROJECT_SOURCE_DIR}/src/samples/lumen_run_validation.cpp"
  "${PROJECT_SOURCE_DIR}/src/samples/showcase_scenes.cpp"
)

set(ASTER_PLATFORM_SOURCES)

if(APPLE)
  list(APPEND ASTER_PLATFORM_SOURCES "${PROJECT_SOURCE_DIR}/src/platform/window_macos.mm")
elseif(WIN32)
  list(APPEND ASTER_PLATFORM_SOURCES "${PROJECT_SOURCE_DIR}/src/platform/window_windows.cpp")
else()
  list(APPEND ASTER_PLATFORM_SOURCES "${PROJECT_SOURCE_DIR}/src/platform/window_linux.cpp")

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
    list(APPEND ASTER_PLATFORM_SOURCES "${protocol_code}" "${protocol_header}")
    set(ASTER_PLATFORM_SOURCES "${ASTER_PLATFORM_SOURCES}" PARENT_SCOPE)
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

set(ASTER_ENGINE_COMPILE_DEFINITIONS
  ASTER_HAS_PROFILING_RUNTIME=$<IF:$<BOOL:${ASTER_ENABLE_PROFILING}>,1,0>
  ASTER_HAS_NETWORK_TRANSPORT=$<IF:$<BOOL:${ASTER_BUILD_NETWORK}>,1,0>
)

function(aster_configure_engine_compile_target target_name)
  target_include_directories(${target_name} PUBLIC
    ${PROJECT_SOURCE_DIR}/include
  )
  if(UNIX AND NOT APPLE)
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
aster_add_engine_object_library(aster_rhi_objects ${ASTER_RHI_SOURCES})
aster_add_engine_object_library(aster_framegraph_objects ${ASTER_FRAMEGRAPH_SOURCES})
aster_add_engine_object_library(aster_asset_objects ${ASTER_ASSET_SOURCES})
aster_add_engine_object_library(aster_material_objects ${ASTER_MATERIAL_SOURCES})
aster_add_engine_object_library(aster_texture_objects ${ASTER_TEXTURE_SOURCES})
aster_add_engine_object_library(aster_shader_objects ${ASTER_SHADER_SOURCES})
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
target_include_directories(aster_game_sdk PUBLIC
  $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
set_target_properties(aster_game_sdk PROPERTIES EXPORT_NAME game_sdk)
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
  target_link_libraries(aster_engine_internal PUBLIC User32 Gdi32 Ws2_32 D3D12 DXGI D3DCompiler)
endif()

target_compile_definitions(aster_engine_internal PUBLIC
  ${ASTER_ENGINE_COMPILE_DEFINITIONS}
)

add_library(aster_kernel SHARED src/kernel/kernel.cpp)
add_library(aster::kernel ALIAS aster_kernel)
target_include_directories(aster_kernel PUBLIC
  $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
target_compile_definitions(aster_kernel PRIVATE ASTER_KERNEL_BUILD=1)
target_link_libraries(aster_kernel PRIVATE aster_engine_internal)
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
install(FILES abi/aster_kernel.symbols DESTINATION ${CMAKE_INSTALL_DATADIR}/aster/abi)
install(EXPORT AsterKernelTargets
  NAMESPACE aster::
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/AsterKernel
)
install(EXPORT AsterGameSdkTargets
  NAMESPACE aster::
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
endif()

if(ASTER_BUILD_PREVIEW)
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
  function(aster_add_test test_name test_source)
    add_executable(${test_name} ${test_source})
    target_link_libraries(${test_name} PRIVATE aster_engine_internal)
    target_include_directories(${test_name} PRIVATE ${PROJECT_SOURCE_DIR}/tests)
    add_test(NAME ${test_name} COMMAND ${test_name})
  endfunction()

  aster_add_test(aster_core_tests tests/core_tests.cpp)
  aster_add_test(aster_math_tests tests/math_tests.cpp)
  aster_add_test(aster_math_property_tests tests/math_property_tests.cpp)
  aster_add_test(aster_geometry_tests tests/geometry_tests.cpp)
  add_executable(aster_asset_cache_tests tests/asset_cache_tests.cpp)
  target_link_libraries(aster_asset_cache_tests PRIVATE aster_engine_internal)
  target_include_directories(aster_asset_cache_tests PRIVATE ${PROJECT_SOURCE_DIR}/tests)
  target_compile_definitions(aster_asset_cache_tests PRIVATE
    ASTER_ASSETC_EXECUTABLE="${ASTER_ASSETC_EXECUTABLE}"
  )
  add_dependencies(aster_asset_cache_tests aster_rust_runtime)
  add_test(NAME aster_asset_cache_tests COMMAND aster_asset_cache_tests)
  add_executable(aster_kernel_public_consumer tests/kernel_public_consumer.cpp)
  target_link_libraries(aster_kernel_public_consumer PRIVATE aster_kernel)
  add_test(NAME aster_kernel_public_consumer COMMAND aster_kernel_public_consumer)

  add_executable(aster_game_sdk_public_consumer tests/game_sdk_public_consumer.cpp)
  target_link_libraries(aster_game_sdk_public_consumer PRIVATE aster_game_sdk)
  target_compile_definitions(aster_game_sdk_public_consumer PRIVATE
    ASTER_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
  )
  add_test(NAME aster_game_sdk_public_consumer COMMAND aster_game_sdk_public_consumer)
  if(ASTER_BUILD_STUDIO)
    add_test(NAME aster_lumen_run_validate_cave
             COMMAND aster_lumen_run --validate-cave)
    add_test(NAME aster_lumen_run_smoke_test
             COMMAND aster_lumen_run --smoke-test --no-vsync)
  endif()

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
      showcase_lab_scenes
      software_preview_renderer
      software_depth_policy_coplanar
      software_depth_policy_object_order
      prepare_scene_custom_mesh_cache
      frame_math_diagnostics
      retro_style_neutral_preview
      retro_style_preview_effects
      retro_style_emissive_gain
      material_render_policies
      runtime_light_policy
      rhi_resource_registry
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
  target_compile_definitions(aster_render_backend_conformance_tests PRIVATE
    ASTER_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
  )
  foreach(render_backend_conformance_case
      software_deterministic_hash_and_forensics
      native_backend_conforms_when_available
      backend_capability_table_contracts
      golden_lab_scenes
      native_lab_scenes_match_software_reference_when_available
      failure_artifact_writer
  )
    add_test(NAME aster_render_backend_conformance_tests.${render_backend_conformance_case}
             COMMAND aster_render_backend_conformance_tests ${render_backend_conformance_case})
  endforeach()

  add_executable(aster_material_shader_system_tests tests/material_shader_system_tests.cpp)
  target_link_libraries(aster_material_shader_system_tests PRIVATE aster_engine_internal)
  target_include_directories(aster_material_shader_system_tests PRIVATE ${PROJECT_SOURCE_DIR}/tests)
  target_compile_definitions(aster_material_shader_system_tests PRIVATE
    ASTER_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
  )
  foreach(material_shader_case
      material_asset_parser_and_compiler
      shader_library_and_reflection
      texture_validation_and_debug
      render_quality_profile
      render_style_profile
      native_render_style_shaders
      hot_reload_snapshot
      invalid_material_diagnostics
  )
    add_test(NAME aster_material_shader_system_tests.${material_shader_case}
             COMMAND aster_material_shader_system_tests ${material_shader_case})
  endforeach()
  aster_add_test(aster_systems_tests tests/systems_tests.cpp)
  aster_add_test(aster_physics_tests tests/physics_tests.cpp)
  aster_add_test(aster_sample_tests tests/sample_tests.cpp)
  if(ASTER_BUILD_NETWORK)
    aster_add_test(aster_network_tests tests/network_tests.cpp)
    add_test(NAME aster_net_probe COMMAND aster_net_probe)
  endif()
endif()
