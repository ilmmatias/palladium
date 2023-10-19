# SPDX-FileCopyrightText: (C) 2023 ilmmatias
# SPDX-License-Identifier: BSD-3-Clause

project(os/drivers/test)

add_executable(test "knostdlib" NO
    main.c)

target_link_libraries(test kernel acpi)
target_include_directories(test PRIVATE include/private PUBLIC include/public)
target_compile_options(test PRIVATE $<$<COMPILE_LANGUAGE:C,CXX>:-ffreestanding>)
target_link_options(test PRIVATE -Wl,--subsystem=native,--entry=DriverEntry)
set_target_properties(test PROPERTIES SUFFIX ".sys")
