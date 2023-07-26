# SPDX-FileCopyrightText: (C) 2023 yuuma03
# SPDX-License-Identifier: BSD-3-Clause

project(os/acpi)

set(SOURCES
    ${SOURCES}
    aml/interpreter.c
    initialize.c)

if(NOT ARCH STREQUAL "amd64")
    include(boot.cmake)
endif()

include(kernel.cmake)
