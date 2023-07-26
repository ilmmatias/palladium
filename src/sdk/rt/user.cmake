# SPDX-FileCopyrightText: (C) 2023 yuuma03
# SPDX-License-Identifier: BSD-3-Clause

add_library(urt "ucrt" SHARED ${SOURCES})
add_import_library(urt urt.def)

target_include_directories(urt PUBLIC include)
target_compile_options(urt PRIVATE $<$<COMPILE_LANGUAGE:C,CXX>:-ffreestanding>)
target_compile_definitions(urt PRIVATE _DLL)
target_link_options(urt PRIVATE -Wl,--entry=DllMain)
