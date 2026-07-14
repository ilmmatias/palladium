# SPDX-FileCopyrightText: (C) 2025-2026 ilmmatias
# SPDX-License-Identifier: GPL-3.0-or-later

add_library(kcrypt "kcrt" STATIC ${SOURCES})
target_include_directories(kcrypt PRIVATE include/private PUBLIC include/public)
