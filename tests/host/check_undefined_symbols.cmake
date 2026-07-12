# SPDX-FileCopyrightText: (C) 2026 ilmmatias
# SPDX-License-Identifier: GPL-3.0-or-later

if(NOT DEFINED NM OR NOT DEFINED ARCHIVE OR NOT DEFINED KIND)
    message(FATAL_ERROR "NM, ARCHIVE, and KIND are required")
endif()

execute_process(
    COMMAND "${NM}" -u "${ARCHIVE}"
    RESULT_VARIABLE result
    OUTPUT_VARIABLE output
    ERROR_VARIABLE error)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "failed to inspect ${ARCHIVE}: ${error}")
endif()

if(KIND STREQUAL "rt")
    set(allowed "^(PalladiumHostMemset|__asan_.*|__ubsan_.*|__stack_chk_fail|__start_asan_globals|__stop_asan_globals)$")
elseif(KIND STREQUAL "crt")
    set(allowed "^(PalladiumIsdigit|PalladiumIslower|PalladiumIsspace|PalladiumIsupper|PalladiumStrtol|__rand64|__rand_state|__srand64|__asan_.*|__ubsan_.*|__stack_chk_fail|__start_asan_globals|__stop_asan_globals)$")
else()
    message(FATAL_ERROR "unknown symbol allowlist kind: ${KIND}")
endif()

string(REPLACE "\n" ";" lines "${output}")
foreach(line IN LISTS lines)
    if(line MATCHES "[ \t]+[UuWw][ \t]+([^ \t]+)$")
        set(symbol "${CMAKE_MATCH_1}")
        if(NOT symbol MATCHES "${allowed}")
            message(FATAL_ERROR "unexpected undefined symbol in ${ARCHIVE}: ${symbol}")
        endif()
    endif()
endforeach()
