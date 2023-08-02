# SPDX-FileCopyrightText: (C) 2023 ilmmatias
# SPDX-License-Identifier: BSD-3-Clause

add_library(ucrt "nostdlib" SHARED ${SOURCES} ucrt.def)
add_import_library(ucrt ucrt.def)

target_include_directories(ucrt PUBLIC include)
target_compile_options(ucrt PRIVATE $<$<COMPILE_LANGUAGE:C,CXX>:-ffreestanding>)
target_compile_definitions(ucrt PRIVATE _DLL)
target_link_options(ucrt PRIVATE -Wl,--entry=CRTDLL_INIT)
