# SPDX-License-Identifier: Apache-2.0
# Copyright (c) 2026 Faruk Alpay

if(NOT DEFINED ASTER_SOURCE_DIR OR NOT DEFINED ASTER_BINARY_DIR OR
   NOT DEFINED ASTER_INSTALL_DIR OR NOT DEFINED ASTER_EXTERNAL_BUILD_DIR)
  message(FATAL_ERROR "ASTER_SOURCE_DIR, ASTER_BINARY_DIR, ASTER_INSTALL_DIR, and ASTER_EXTERNAL_BUILD_DIR are required")
endif()

set(config_args "")
if(DEFINED ASTER_CONFIG AND NOT ASTER_CONFIG STREQUAL "")
  list(APPEND config_args --config "${ASTER_CONFIG}")
endif()

file(REMOVE_RECURSE "${ASTER_INSTALL_DIR}" "${ASTER_EXTERNAL_BUILD_DIR}")

foreach(public_target aster_kernel aster_game_sdk)
  execute_process(
    COMMAND "${CMAKE_COMMAND}" --build "${ASTER_BINARY_DIR}" --target ${public_target} ${config_args}
    RESULT_VARIABLE build_result
  )
  if(NOT build_result EQUAL 0)
    message(FATAL_ERROR "Building ${public_target} before install-tree smoke test failed")
  endif()
endforeach()

execute_process(
  COMMAND "${CMAKE_COMMAND}" --install "${ASTER_BINARY_DIR}" --prefix "${ASTER_INSTALL_DIR}" ${config_args}
  RESULT_VARIABLE install_result
)
if(NOT install_result EQUAL 0)
  message(FATAL_ERROR "Installing Aster public SDK failed")
endif()

foreach(public_dir kernel game_sdk)
  if(NOT EXISTS "${ASTER_INSTALL_DIR}/include/aster/${public_dir}")
    message(FATAL_ERROR "Public header directory was not installed: include/aster/${public_dir}")
  endif()
endforeach()

foreach(private_dir render rhi framegraph scene material)
  if(EXISTS "${ASTER_INSTALL_DIR}/include/aster/${private_dir}")
    message(FATAL_ERROR "Private header directory was installed: include/aster/${private_dir}")
  endif()
endforeach()

foreach(package_name AsterKernel AsterGameSdk)
  file(GLOB_RECURSE package_configs
    "${ASTER_INSTALL_DIR}/*/cmake/${package_name}/${package_name}Config.cmake")
  if(package_configs STREQUAL "")
    message(FATAL_ERROR "${package_name} package config was not installed")
  endif()
endforeach()

execute_process(
  COMMAND "${CMAKE_COMMAND}"
    -S "${ASTER_SOURCE_DIR}/external_app_minimal"
    -B "${ASTER_EXTERNAL_BUILD_DIR}"
    "-DCMAKE_PREFIX_PATH=${ASTER_INSTALL_DIR}"
    -DCMAKE_BUILD_TYPE=RelWithDebInfo
  RESULT_VARIABLE configure_result
)
if(NOT configure_result EQUAL 0)
  message(FATAL_ERROR "Configuring external_app_minimal from install tree failed")
endif()

execute_process(
  COMMAND "${CMAKE_COMMAND}" --build "${ASTER_EXTERNAL_BUILD_DIR}" ${config_args}
  RESULT_VARIABLE external_build_result
)
if(NOT external_build_result EQUAL 0)
  message(FATAL_ERROR "Building external_app_minimal from install tree failed")
endif()

set(game_sdk_consumer_source_dir "${ASTER_EXTERNAL_BUILD_DIR}/game_sdk_consumer_source")
set(game_sdk_consumer_build_dir "${ASTER_EXTERNAL_BUILD_DIR}/game_sdk_consumer_build")
file(MAKE_DIRECTORY "${game_sdk_consumer_source_dir}")
file(WRITE "${game_sdk_consumer_source_dir}/CMakeLists.txt"
"cmake_minimum_required(VERSION 3.24)
project(AsterGameSdkInstallConsumer LANGUAGES CXX)
find_package(AsterGameSdk CONFIG REQUIRED)
add_executable(game_sdk_consumer main.cpp)
target_link_libraries(game_sdk_consumer PRIVATE aster::game_sdk)
")
file(WRITE "${game_sdk_consumer_source_dir}/main.cpp"
"#include <aster/game_sdk/game_sdk.hpp>
#include <cassert>
int main() {
  const auto project = aster::sdk::parseProjectDocument(R\"json({
    \"schema_version\": 1,
    \"name\": \"install-consumer\",
    \"startup_scene\": \"scene.install\",
    \"assets\": [
      { \"id\": \"scene.install\", \"kind\": \"scene\", \"path\": \"scenes/install.scene\" }
    ]
  })json\");
  assert(project.ok());
  assert(project.value.name == \"install-consumer\");
  return 0;
}
")

execute_process(
  COMMAND "${CMAKE_COMMAND}"
    -S "${game_sdk_consumer_source_dir}"
    -B "${game_sdk_consumer_build_dir}"
    "-DCMAKE_PREFIX_PATH=${ASTER_INSTALL_DIR}"
    -DCMAKE_BUILD_TYPE=RelWithDebInfo
  RESULT_VARIABLE game_sdk_configure_result
)
if(NOT game_sdk_configure_result EQUAL 0)
  message(FATAL_ERROR "Configuring game_sdk_consumer from install tree failed")
endif()

execute_process(
  COMMAND "${CMAKE_COMMAND}" --build "${game_sdk_consumer_build_dir}" ${config_args}
  RESULT_VARIABLE game_sdk_external_build_result
)
if(NOT game_sdk_external_build_result EQUAL 0)
  message(FATAL_ERROR "Building game_sdk_consumer from install tree failed")
endif()

set(external_exe_name external_app_minimal)
if(WIN32)
  set(external_exe_name external_app_minimal.exe)
endif()

set(external_exe_candidates)
if(DEFINED ASTER_CONFIG AND NOT ASTER_CONFIG STREQUAL "")
  list(APPEND external_exe_candidates
    "${ASTER_EXTERNAL_BUILD_DIR}/${ASTER_CONFIG}/${external_exe_name}")
endif()
list(APPEND external_exe_candidates "${ASTER_EXTERNAL_BUILD_DIR}/${external_exe_name}")

set(external_exe "")
foreach(candidate IN LISTS external_exe_candidates)
  if(EXISTS "${candidate}")
    set(external_exe "${candidate}")
    break()
  endif()
endforeach()
if(external_exe STREQUAL "")
  message(FATAL_ERROR "Could not locate external_app_minimal executable after build")
endif()

if(APPLE)
  set(library_path_name DYLD_LIBRARY_PATH)
  set(library_path_value "${ASTER_INSTALL_DIR}/lib:$ENV{${library_path_name}}")
elseif(WIN32)
  set(library_path_name PATH)
  set(library_path_value "${ASTER_INSTALL_DIR}/bin;$ENV{${library_path_name}}")
else()
  set(library_path_name LD_LIBRARY_PATH)
  set(library_path_value "${ASTER_INSTALL_DIR}/lib:$ENV{${library_path_name}}")
endif()

execute_process(
  COMMAND "${CMAKE_COMMAND}" -E env
    "${library_path_name}=${library_path_value}"
    "${external_exe}"
  RESULT_VARIABLE run_result
)
if(NOT run_result EQUAL 0)
  message(FATAL_ERROR "Running external_app_minimal from install tree failed")
endif()
