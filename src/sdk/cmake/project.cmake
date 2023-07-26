# SPDX-FileCopyrightText: (C) 2023 yuuma03
# SPDX-License-Identifier: BSD-3-Clause

function(add_executable target type)
    _add_executable(${target} ${ARGN})

    if(NOT type STREQUAL "nostdlib")
        if(type STREQUAL "kcrt")
            target_link_libraries(${target} kcrt)
        elseif(type STREQUAL "ucrt")
            target_link_libraries(${target} ucrt)
        elseif(type STREQUAL "kstdlib")
            target_link_libraries(${target} kcrt krt)
        else()
            target_link_libraries(${target} ucrt urt)
        endif()
    endif()

    target_compile_options(${target} PRIVATE --target=${TARGET_${ARCH}}-w64-mingw32)
    target_link_options(${target} PRIVATE --target=${TARGET_${ARCH}}-w64-mingw32 -fuse-ld=lld)
endfunction()

function(add_library target type)
    _add_library(${target} ${ARGN})

    if(NOT type STREQUAL "nostdlib")
        if(type STREQUAL "kcrt")
            target_link_libraries(${target} kcrt)
        elseif(type STREQUAL "ucrt")
            target_link_libraries(${target} ucrt)
        elseif(type STREQUAL "kstdlib")
            target_link_libraries(${target} kcrt krt)
        else()
            target_link_libraries(${target} ucrt urt)
        endif()
    endif()

    target_compile_options(${target} PRIVATE --target=${TARGET_${ARCH}}-w64-mingw32)
    target_link_options(${target} PRIVATE --target=${TARGET_${ARCH}}-w64-mingw32 -fuse-ld=lld)
endfunction()

function(add_import_library target def_file)
    add_custom_command(
        OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${target}.lib
        COMMAND lld-link /def:${CMAKE_CURRENT_SOURCE_DIR}/${def_file}
                         /out:${CMAKE_CURRENT_BINARY_DIR}/${target}.lib /machine:${ARCH}
        DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/${def_file}
    )

    add_custom_target(${target}lib ALL DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/${target}.lib)
endfunction()
