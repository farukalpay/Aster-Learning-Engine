# Author: Faruk Alpay
# Do not remove this notice.

if(NOT DEFINED ASTER_SOURCE_DIR OR NOT DEFINED ASTER_BINARY_DIR OR
   NOT DEFINED ASTER_INSTALL_DIR OR NOT DEFINED ASTER_EXTERNAL_BUILD_DIR)
  message(FATAL_ERROR "ASTER_SOURCE_DIR, ASTER_BINARY_DIR, ASTER_INSTALL_DIR, and ASTER_EXTERNAL_BUILD_DIR are required")
endif()

set(config_args "")
if(DEFINED ASTER_CONFIG AND NOT ASTER_CONFIG STREQUAL "")
  list(APPEND config_args --config "${ASTER_CONFIG}")
endif()

file(REMOVE_RECURSE "${ASTER_INSTALL_DIR}" "${ASTER_EXTERNAL_BUILD_DIR}")

execute_process(
  COMMAND "${CMAKE_COMMAND}" --build "${ASTER_BINARY_DIR}" --target aster_kernel ${config_args}
  RESULT_VARIABLE build_result
)
if(NOT build_result EQUAL 0)
  message(FATAL_ERROR "Building aster_kernel before install-tree smoke test failed")
endif()

execute_process(
  COMMAND "${CMAKE_COMMAND}" --install "${ASTER_BINARY_DIR}" --prefix "${ASTER_INSTALL_DIR}" ${config_args}
  RESULT_VARIABLE install_result
)
if(NOT install_result EQUAL 0)
  message(FATAL_ERROR "Installing Aster kernel SDK failed")
endif()

foreach(private_dir render rhi framegraph scene material)
  if(EXISTS "${ASTER_INSTALL_DIR}/include/aster/${private_dir}")
    message(FATAL_ERROR "Private header directory was installed: include/aster/${private_dir}")
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

set(external_exe "${ASTER_EXTERNAL_BUILD_DIR}/external_app_minimal")
if(DEFINED ASTER_CONFIG AND NOT ASTER_CONFIG STREQUAL "")
  set(config_exe "${ASTER_EXTERNAL_BUILD_DIR}/${ASTER_CONFIG}/external_app_minimal")
  if(EXISTS "${config_exe}" OR WIN32)
    set(external_exe "${config_exe}")
  endif()
endif()
if(WIN32)
  set(external_exe "${external_exe}.exe")
endif()

if(APPLE)
  set(library_path_name DYLD_LIBRARY_PATH)
elseif(WIN32)
  set(library_path_name PATH)
else()
  set(library_path_name LD_LIBRARY_PATH)
endif()

execute_process(
  COMMAND "${CMAKE_COMMAND}" -E env
    "${library_path_name}=${ASTER_INSTALL_DIR}/lib:$ENV{${library_path_name}}"
    "${external_exe}"
  RESULT_VARIABLE run_result
)
if(NOT run_result EQUAL 0)
  message(FATAL_ERROR "Running external_app_minimal from install tree failed")
endif()
