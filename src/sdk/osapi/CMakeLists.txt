# SPDX-FileCopyrightText: (C) 2025 ilmmatias
# SPDX-License-Identifier: GPL-3.0-or-later

project(os/sdk/osapi)

set(SOURCES
    startup.c)

add_library(osapi "nostdlib" SHARED ${SOURCES} osapi.def)
target_include_directories(osapi PUBLIC include)
target_compile_options(osapi PRIVATE $<$<COMPILE_LANGUAGE:C>:-ffreestanding>)
target_link_options(osapi PRIVATE -Wl,--entry=OsApiDllStartup)
