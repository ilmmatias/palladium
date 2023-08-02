# SPDX-FileCopyrightText: (C) 2023 ilmmatias
# SPDX-License-Identifier: BSD-3-Clause

add_library(kacpi "knostdlib" STATIC ${SOURCES})

target_include_directories(kacpi PUBLIC include)
target_compile_options(kacpi PRIVATE $<$<COMPILE_LANGUAGE:C,CXX>:-ffreestanding>)
target_link_libraries(kacpi PRIVATE kcrt)
