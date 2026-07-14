# SPDX-FileCopyrightText: (C) 2023-2026 ilmmatias
# SPDX-License-Identifier: GPL-3.0-or-later

add_library(kcrt "knostdlib" STATIC ${SOURCES})
target_include_directories(kcrt PUBLIC include)

add_library(kcrt_compiler "knostdlib" STATIC ${COMPILER_SOURCES})
target_include_directories(kcrt_compiler PRIVATE include)
