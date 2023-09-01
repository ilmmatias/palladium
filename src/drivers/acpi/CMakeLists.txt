# SPDX-FileCopyrightText: (C) 2023 ilmmatias
# SPDX-License-Identifier: BSD-3-Clause

project(os/drivers/acpi)

add_executable(acpi "knostdlib"
    aml.c
    main.c
    rsdt.c
    termarg.c
    termlist.c)

target_link_libraries(acpi kernel)
target_include_directories(acpi PRIVATE include)
target_compile_options(acpi PRIVATE $<$<COMPILE_LANGUAGE:C,CXX>:-ffreestanding>)
target_link_options(acpi PRIVATE -Wl,--subsystem=native,--entry=DriverEntry)
set_target_properties(acpi PROPERTIES SUFFIX ".sys")
