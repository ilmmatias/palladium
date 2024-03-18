# SPDX-FileCopyrightText: (C) 2023-2024 ilmmatias
# SPDX-License-Identifier: GPL-3.0-or-later

add_library(urt "ucrt" SHARED ${SOURCES})

target_include_directories(urt PUBLIC include)
target_compile_options(urt PRIVATE $<$<COMPILE_LANGUAGE:C,CXX>:-ffreestanding>)
target_compile_definitions(urt PRIVATE _DLL)
target_link_options(urt PRIVATE -Wl,--entry=DllMain)
