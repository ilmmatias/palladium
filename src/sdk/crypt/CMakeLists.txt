# SPDX-FileCopyrightText: (C) 2025 ilmmatias
# SPDX-License-Identifier: GPL-3.0-or-later

project(os/sdk/crypt)

set(SOURCES
    sha2/sha224.c
    sha2/sha256.c
    sha2/sha384.c
    sha2/sha512.c

    hash.c
    hmac.c
    chacha20.c)

include(kernel.cmake)
include(user.cmake)
