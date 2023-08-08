# SPDX-FileCopyrightText: (C) 2023 ilmmatias
# SPDX-License-Identifier: BSD-3-Clause

add_library(biso9660fs "kstdlib" STATIC ${SOURCES})

target_include_directories(biso9660fs PUBLIC include/public)
target_include_directories(biso9660fs PRIVATE include/private)
target_compile_options(biso9660fs PRIVATE $<$<COMPILE_LANGUAGE:C,CXX>:-ffreestanding>)
