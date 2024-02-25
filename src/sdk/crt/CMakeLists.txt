# SPDX-FileCopyrightText: (C) 2023 ilmmatias
# SPDX-License-Identifier: GPL-3.0-or-later

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

    stdio/__vprintf.c
    stdio/__vscanf.c
    stdio/snprintf.c
    stdio/sprintf.c
    stdio/sscanf.c
    stdio/vsnprintf.c
    stdio/vsprintf.c
    stdio/vsscanf.c

    stdlib/__rand64.c
    stdlib/__srand64.c
    stdlib/atof.c
    stdlib/atoi.c
    stdlib/atol.c
    stdlib/atoll.c
    stdlib/rand.c
    stdlib/srand.c
    stdlib/strtod.c
    stdlib/strtof.c
    stdlib/strtol.c
    stdlib/strtold.c
    stdlib/strtoll.c
    stdlib/strtoul.c
    stdlib/strtoull.c

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
include(user.cmake)
