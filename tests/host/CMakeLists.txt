cmake_minimum_required(VERSION 3.21)

project(palladium-host-tests LANGUAGES C)

include(CTest)

set(PALLADIUM_ROOT "${CMAKE_CURRENT_LIST_DIR}/../..")

add_library(
    palladium-portable-rt STATIC
    ${PALLADIUM_ROOT}/src/sdk/rt/avltree.c
    ${PALLADIUM_ROOT}/src/sdk/rt/bitmap.c
    ${PALLADIUM_ROOT}/src/sdk/rt/hash.c
    ${PALLADIUM_ROOT}/src/sdk/rt/list.c)

set_source_files_properties(
    ${PALLADIUM_ROOT}/src/sdk/rt/bitmap.c PROPERTIES COMPILE_DEFINITIONS
                                                        "memset=PalladiumHostMemset")

target_compile_features(palladium-portable-rt PRIVATE c_std_23)
target_include_directories(
    palladium-portable-rt
    PRIVATE ${PALLADIUM_ROOT}/src/sdk/rt/include ${PALLADIUM_ROOT}/src/sdk/include)
target_compile_options(
    palladium-portable-rt
    PRIVATE -Wall
            -Wextra
            -Werror
            -ffreestanding
            -fno-builtin
            -fno-omit-frame-pointer
            -fsanitize=address,undefined)
add_custom_command(
    TARGET palladium-portable-rt
    POST_BUILD
    COMMAND
        ${CMAKE_COMMAND} -DNM=${CMAKE_NM} -DARCHIVE=$<TARGET_FILE:palladium-portable-rt>
        -DKIND=rt -P ${CMAKE_CURRENT_LIST_DIR}/check_undefined_symbols.cmake)

add_executable(rt-host-tests rt_tests.c)
target_compile_features(rt-host-tests PRIVATE c_std_23)
target_include_directories(
    rt-host-tests
    PRIVATE ${PALLADIUM_ROOT}/src/sdk/rt/include ${PALLADIUM_ROOT}/src/sdk/include)
target_compile_options(
    rt-host-tests
    PRIVATE -Wall -Wextra -Werror -fno-omit-frame-pointer -fsanitize=address,undefined)
target_link_options(rt-host-tests PRIVATE -fsanitize=address,undefined)
target_link_libraries(rt-host-tests PRIVATE palladium-portable-rt)

add_test(NAME rt-host-tests COMMAND rt-host-tests)
set_tests_properties(rt-host-tests PROPERTIES ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0")

add_library(
    palladium-portable-crt STATIC
    ${PALLADIUM_ROOT}/src/sdk/crt/stdio/__vscanf.c
    ${PALLADIUM_ROOT}/src/sdk/crt/stdlib/__rand64.c
    ${PALLADIUM_ROOT}/src/sdk/crt/stdlib/__srand64.c
    ${PALLADIUM_ROOT}/src/sdk/crt/stdlib/rand.c
    ${PALLADIUM_ROOT}/src/sdk/crt/stdlib/srand.c)
target_compile_features(palladium-portable-crt PRIVATE c_std_23)
target_include_directories(
    palladium-portable-crt
    PRIVATE ${PALLADIUM_ROOT}/src/sdk/crt/include ${PALLADIUM_ROOT}/src/sdk/include)
target_compile_definitions(
    palladium-portable-crt
    PRIVATE rand=PalladiumRand
            srand=PalladiumSrand
            strtol=PalladiumStrtol
            isspace=PalladiumIsspace
            isdigit=PalladiumIsdigit
            islower=PalladiumIslower
            isupper=PalladiumIsupper)
target_compile_options(
    palladium-portable-crt
    PRIVATE -Wall
            -Wextra
            -Werror
            -ffreestanding
            -fno-builtin
            -fno-omit-frame-pointer
            -fsanitize=address,undefined)
add_custom_command(
    TARGET palladium-portable-crt
    POST_BUILD
    COMMAND
        ${CMAKE_COMMAND} -DNM=${CMAKE_NM} -DARCHIVE=$<TARGET_FILE:palladium-portable-crt>
        -DKIND=crt -P ${CMAKE_CURRENT_LIST_DIR}/check_undefined_symbols.cmake)

add_executable(crt-host-tests crt_tests.c)
target_compile_features(crt-host-tests PRIVATE c_std_23)
target_compile_options(
    crt-host-tests
    PRIVATE -Wall -Wextra -Werror -fno-omit-frame-pointer -fsanitize=address,undefined)
target_link_options(crt-host-tests PRIVATE -fsanitize=address,undefined)
target_link_libraries(crt-host-tests PRIVATE palladium-portable-crt)
add_test(NAME crt-host-tests COMMAND crt-host-tests)
set_tests_properties(crt-host-tests PROPERTIES ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0")
