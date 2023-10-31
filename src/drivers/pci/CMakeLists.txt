# SPDX-FileCopyrightText: (C) 2023 ilmmatias
# SPDX-License-Identifier: BSD-3-Clause

project(os/drivers/pci)

add_executable(pci "knostdlib" YES
    bus.c
    debug.c
    main.c
    pci.def)

target_link_libraries(pci acpi kernel)
target_include_directories(pci PRIVATE include/private PUBLIC include/public)
target_compile_options(pci PRIVATE $<$<COMPILE_LANGUAGE:C,CXX>:-ffreestanding>)
target_link_options(pci PRIVATE -Wl,--subsystem=native,--entry=DriverEntry)
set_target_properties(pci PROPERTIES SUFFIX ".sys")
set_target_properties(pci PROPERTIES ENABLE_EXPORTS TRUE)
