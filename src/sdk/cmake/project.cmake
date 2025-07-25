# SPDX-FileCopyrightText: (C) 2023-2025 ilmmatias
# SPDX-License-Identifier: GPL-3.0-or-later

function(add_executable target type has_lib)
    _add_executable(${target} ${ARGN})

    if(NOT (type STREQUAL "nostdlib" OR type STREQUAL "knostdlib"))
        if(type STREQUAL "kcrt")
            target_link_libraries(${target} PRIVATE kcrt)
        elseif(type STREQUAL "kstdlib")
            target_link_libraries(${target} PRIVATE kcrt krt)
        elseif(type STREQUAL "bstdlib")
            target_link_libraries(${target} PRIVATE kcrt brt)
        elseif(type STREQUAL "kernel")
            target_link_libraries(${target} PRIVATE kcrt krt)
            target_include_directories(${target} INTERFACE $<TARGET_PROPERTY:kcrt,INCLUDE_DIRECTORIES>)
            target_include_directories(${target} INTERFACE $<TARGET_PROPERTY:krt,INCLUDE_DIRECTORIES>)
        elseif(type STREQUAL "osapi")
            target_link_libraries(${target} PRIVATE osapi)
        elseif(type STREQUAL "ucrt")
            target_link_libraries(${target} PRIVATE osapi ucrt ucrt_exe_startup)
        else()
            target_link_libraries(${target} PRIVATE osapi ucrt ucrt_exe_startup urt)
        endif()
    endif()

    # amd64 depends on cmpxchg16b (for lock-free SLists).
    if (ARCH STREQUAL "amd64")
        target_compile_options(${target} PRIVATE -mcx16)
    endif()

    if((ARCH STREQUAL "amd64" OR ARCH STREQUAL "x86") AND
       (type STREQUAL "kcrt" OR type STREQUAL "kstdlib" OR type STREQUAL "bstdlib" OR
        type STREQUAL "knostdlib" OR type STREQUAL "kernel"))
        target_compile_options(
            ${target}
            PRIVATE
            $<$<COMPILE_LANGUAGE:C>:-mno-red-zone>)
    endif()

    target_compile_options(
        ${target}
        PRIVATE
        --target=${TARGET_${ARCH}}-w64-mingw32
        -DARCH_${ARCH}
        -DARCH_MAKE_INCLUDE_PATH\(PREFIX,SUFFIX\)=<PREFIX/${ARCH}/SUFFIX>
        -fstack-protector-strong
        -mno-stack-arg-probe
        -fexceptions
        -fseh-exceptions
        -fms-extensions
        -include ${CMAKE_SOURCE_DIR}/sdk/include/os/intellisense.h)

    target_link_options(
    	${target}
    	PRIVATE
    	--target=${TARGET_${ARCH}}-w64-mingw32
    	-fuse-ld=lld
    	/usr/local/lib/baremetal/libclang_rt.builtins-${TARGET_${ARCH}}.a)

    target_include_directories(
        ${target}
        PRIVATE
        ${CMAKE_SOURCE_DIR}/sdk/include)

    if(has_lib)
        target_link_options(${target} PRIVATE -Wl,--out-implib,${CMAKE_CURRENT_BINARY_DIR}/${target}.lib)
    endif()
endfunction()

function(add_library target type)
    _add_library(${target} ${ARGN})

    if(NOT (type STREQUAL "nostdlib" OR type STREQUAL "knostdlib"))
        if(type STREQUAL "kcrt")
            target_link_libraries(${target} PRIVATE kcrt)
        elseif(type STREQUAL "kstdlib")
            target_link_libraries(${target} PRIVATE kcrt krt)
        elseif(type STREQUAL "bstdlib")
            target_link_libraries(${target} PRIVATE kcrt brt)
        elseif(type STREQUAL "kernel")
            target_link_libraries(${target} PRIVATE kcrt krt)
            target_include_directories(${target} INTERFACE $<TARGET_PROPERTY:kcrt,INCLUDE_DIRECTORIES>)
            target_include_directories(${target} INTERFACE $<TARGET_PROPERTY:krt,INCLUDE_DIRECTORIES>)
        elseif(type STREQUAL "osapi")
            target_link_libraries(${target} PRIVATE osapi)
        elseif(type STREQUAL "ucrt")
            target_link_libraries(${target} PRIVATE osapi ucrt ucrt_dll_startup)
        else()
            target_link_libraries(${target} PRIVATE osapi ucrt ucrt_dll_startup urt)
        endif()
    endif()

    # amd64 depends on cmpxchg16b (for lock-free SLists).
    if (ARCH STREQUAL "amd64")
        target_compile_options(${target} PRIVATE -mcx16)
    endif()

    if((ARCH STREQUAL "amd64" OR ARCH STREQUAL "x86") AND
       (type STREQUAL "kcrt" OR type STREQUAL "kstdlib" OR type STREQUAL "bstdlib" OR
        type STREQUAL "knostdlib" OR type STREQUAL "kernel"))
        target_compile_options(
            ${target}
            PRIVATE
            $<$<COMPILE_LANGUAGE:C>:-mno-red-zone>)
    endif()

    target_compile_options(
        ${target}
        PRIVATE
        --target=${TARGET_${ARCH}}-w64-mingw32
        -DARCH_${ARCH}
        -DARCH_MAKE_INCLUDE_PATH\(PREFIX,SUFFIX\)=<PREFIX/${ARCH}/SUFFIX>
        -fstack-protector-strong
        -mno-stack-arg-probe
        -fexceptions
        -fseh-exceptions
        -fms-extensions
        -include ${CMAKE_SOURCE_DIR}/sdk/include/os/intellisense.h)

    target_link_options(
        	${target}
        	PRIVATE
        	--target=${TARGET_${ARCH}}-w64-mingw32
            -Wl,--out-implib,${CMAKE_CURRENT_BINARY_DIR}/${target}.lib
        	-fuse-ld=lld
        	/usr/local/lib/baremetal/libclang_rt.builtins-${TARGET_${ARCH}}.a)
    
    target_include_directories(
        ${target}
        PRIVATE
        ${CMAKE_SOURCE_DIR}/sdk/include)
endfunction()
