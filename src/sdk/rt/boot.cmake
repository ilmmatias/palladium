# SPDX-FileCopyrightText: (C) 2023-2025 ilmmatias
# SPDX-License-Identifier: GPL-3.0-or-later

add_library(brt "kcrt" STATIC ${SOURCES})
target_include_directories(brt PUBLIC include)
target_compile_options(brt PRIVATE $<$<COMPILE_LANGUAGE:C>:-ffreestanding>)
