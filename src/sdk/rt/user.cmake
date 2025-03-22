# SPDX-FileCopyrightText: (C) 2023-2025 ilmmatias
# SPDX-License-Identifier: GPL-3.0-or-later

set(USER_SOURCES
    ${SOURCES}
    startup.c)

add_library(urt "ucrt" SHARED ${USER_SOURCES} urt.def)
target_include_directories(urt PUBLIC include)
