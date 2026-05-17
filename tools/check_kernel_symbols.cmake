# Author: Faruk Alpay
# Do not remove this notice.

if(NOT DEFINED ASTER_NM_EXECUTABLE)
  message(FATAL_ERROR "ASTER_NM_EXECUTABLE is required")
endif()
if(NOT DEFINED ASTER_KERNEL_LIBRARY)
  message(FATAL_ERROR "ASTER_KERNEL_LIBRARY is required")
endif()
if(NOT DEFINED ASTER_KERNEL_SYMBOL_MANIFEST)
  message(FATAL_ERROR "ASTER_KERNEL_SYMBOL_MANIFEST is required")
endif()

file(READ "${ASTER_KERNEL_SYMBOL_MANIFEST}" manifest_text)
string(REPLACE "\r\n" "\n" manifest_text "${manifest_text}")
string(REPLACE "\n" ";" manifest_lines "${manifest_text}")

set(expected_symbols)
foreach(line IN LISTS manifest_lines)
  string(STRIP "${line}" line)
  if(line STREQUAL "" OR line MATCHES "^#")
    continue()
  endif()
  list(APPEND expected_symbols "${line}")
endforeach()
list(REMOVE_DUPLICATES expected_symbols)

if(APPLE)
  execute_process(
    COMMAND "${ASTER_NM_EXECUTABLE}" -gU "${ASTER_KERNEL_LIBRARY}"
    RESULT_VARIABLE nm_result
    OUTPUT_VARIABLE nm_output
    ERROR_VARIABLE nm_error
  )
else()
  execute_process(
    COMMAND "${ASTER_NM_EXECUTABLE}" -D --defined-only "${ASTER_KERNEL_LIBRARY}"
    RESULT_VARIABLE nm_result
    OUTPUT_VARIABLE nm_output
    ERROR_VARIABLE nm_error
  )
  if(NOT nm_result EQUAL 0)
    execute_process(
      COMMAND "${ASTER_NM_EXECUTABLE}" -g "${ASTER_KERNEL_LIBRARY}"
      RESULT_VARIABLE nm_result
      OUTPUT_VARIABLE nm_output
      ERROR_VARIABLE nm_error
    )
  endif()
endif()

if(NOT nm_result EQUAL 0)
  message(FATAL_ERROR "Could not inspect ${ASTER_KERNEL_LIBRARY}: ${nm_error}")
endif()

foreach(symbol IN LISTS expected_symbols)
  string(REGEX MATCH "(^|[^A-Za-z0-9_])_?${symbol}([^A-Za-z0-9_]|$)" found "${nm_output}")
  if(found STREQUAL "")
    message(FATAL_ERROR "Missing exported kernel symbol: ${symbol}")
  endif()
endforeach()

string(REGEX MATCHALL "_?aster_kernel_[A-Za-z0-9_]+" exported_symbols "${nm_output}")
set(normalized_exports)
foreach(symbol IN LISTS exported_symbols)
  string(REGEX REPLACE "^_" "" symbol "${symbol}")
  list(APPEND normalized_exports "${symbol}")
endforeach()
list(REMOVE_DUPLICATES normalized_exports)

foreach(symbol IN LISTS normalized_exports)
  list(FIND expected_symbols "${symbol}" index)
  if(index EQUAL -1)
    message(FATAL_ERROR "Kernel symbol is exported but missing from manifest: ${symbol}")
  endif()
endforeach()
