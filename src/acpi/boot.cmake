# SPDX-FileCopyrightText: (C) 2023 yuuma03
# SPDX-License-Identifier: BSD-3-Clause

add_library(bacpi "nostdlib" STATIC ${SOURCES})

target_include_directories(bacpi PUBLIC include)
target_compile_options(bacpi PRIVATE $<$<COMPILE_LANGUAGE:C,CXX>:-ffreestanding>)
target_link_libraries(bacpi PRIVATE kcrt)
