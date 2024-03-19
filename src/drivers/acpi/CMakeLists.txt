# SPDX-FileCopyrightText: (C) 2023-2024 ilmmatias
# SPDX-License-Identifier: GPL-3.0-or-later

project(os/drivers/acpi)
add_executable(acpi "knostdlib" YES
    ${SOURCES}

    acpi.def
    main.cxx
    object.cxx
    os.cxx)

target_link_libraries(acpi kernel)
target_include_directories(acpi PRIVATE include/private PUBLIC include/public)
target_compile_options(acpi PRIVATE $<$<COMPILE_LANGUAGE:C,CXX>:-ffreestanding>)
target_link_options(acpi PRIVATE -Wl,--subsystem=native,--entry=DriverEntry)
set_target_properties(acpi PROPERTIES SUFFIX ".sys")
set_target_properties(acpi PROPERTIES ENABLE_EXPORTS TRUE)
