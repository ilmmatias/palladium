# SPDX-FileCopyrightText: (C) 2023 ilmmatias
# SPDX-License-Identifier: BSD-3-Clause

function(add_executable target type)
    _add_executable(${target} ${ARGN})

    if(NOT (type STREQUAL "nostdlib" OR type STREQUAL "knostdlib"))
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

    if((ARCH STREQUAL "amd64" OR ARCH STREQUAL "x86") AND
       (type STREQUAL "kcrt" OR type STREQUAL "kstdlib" OR type STREQUAL "knostdlib"))
        target_compile_options(
            ${target}
            PRIVATE
            $<$<COMPILE_LANGUAGE:C,CXX>:-mno-mmx>
            $<$<COMPILE_LANGUAGE:C,CXX>:-mno-sse>
            $<$<COMPILE_LANGUAGE:C,CXX>:-mno-sse2>)
    endif()

    target_compile_options(${target} PRIVATE --target=${TARGET_${ARCH}}-w64-mingw32 -DARCH_${ARCH})
    target_link_options(
    	${target}
    	PRIVATE
    	--target=${TARGET_${ARCH}}-w64-mingw32
    	-fuse-ld=lld
    	/usr/local/lib/baremetal/libclang_rt.builtins-${TARGET_${ARCH}}.a)
endfunction()

function(add_library target type)
    _add_library(${target} ${ARGN})

    if(NOT (type STREQUAL "nostdlib" OR type STREQUAL "knostdlib"))
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

    if((ARCH STREQUAL "amd64" OR ARCH STREQUAL "x86") AND
       (type STREQUAL "kcrt" OR type STREQUAL "kstdlib" OR type STREQUAL "knostdlib"))
        target_compile_options(
            ${target}
            PRIVATE
            $<$<COMPILE_LANGUAGE:C,CXX>:-mno-mmx>
            $<$<COMPILE_LANGUAGE:C,CXX>:-mno-sse>
            $<$<COMPILE_LANGUAGE:C,CXX>:-mno-sse2>)
    endif()

    target_compile_options(${target} PRIVATE --target=${TARGET_${ARCH}}-w64-mingw32 -DARCH_${ARCH})
    target_link_options(
        	${target}
        	PRIVATE
        	--target=${TARGET_${ARCH}}-w64-mingw32
            -Wl,--out-implib,${CMAKE_CURRENT_BINARY_DIR}/${target}.lib
        	-fuse-ld=lld
        	/usr/local/lib/baremetal/libclang_rt.builtins-${TARGET_${ARCH}}.a)
endfunction()
