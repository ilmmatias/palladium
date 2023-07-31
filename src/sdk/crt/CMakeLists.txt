# SPDX-FileCopyrightText: (C) 2023 yuuma03
# SPDX-License-Identifier: BSD-3-Clause

project(os/sdk/crt)

set(SOURCES
    placeholder.c

    ctype/isalnum.c
    ctype/isalpha.c
    ctype/isblank.c
    ctype/iscntrl.c
    ctype/isdigit.c
    ctype/isgraph.c
    ctype/islower.c
    ctype/isprint.c
    ctype/ispunct.c
    ctype/isspace.c
    ctype/isupper.c
    ctype/isxdigit.c
    ctype/tolower.c
    ctype/toupper.c

    string/memccpy.c
    string/memchr.c
    string/memcmp.c
    string/memcpy.c
    string/memmove.c
    string/memset.c
    string/strcat.c
    string/strchr.c
    string/strcmp.c
    string/strcpy.c
    string/strcspn.c
    string/strlen.c
    string/strncat.c
    string/strncmp.c
    string/strncpy.c
    string/strpbrk.c
    string/strrchr.c
    string/strspn.c
    string/strstr.c
    string/strtok.c)

include(kernel.cmake)

if(NOT ARCH STREQUAL "x86")
    include(user.cmake)
endif()
