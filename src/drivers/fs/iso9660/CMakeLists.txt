# SPDX-FileCopyrightText: (C) 2023 ilmmatias
# SPDX-License-Identifier: BSD-3-Clause

project(os/drivers/fs/iso9660)

set(SOURCES
    main.c)

if(NOT ARCH STREQUAL "amd64")
    include(boot.cmake)
endif()
