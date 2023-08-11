# SPDX-FileCopyrightText: (C) 2023 ilmmatias
# SPDX-License-Identifier: BSD-3-Clause

set(KERNEL_SOURCES
    ${SOURCES}

    stdio/__parse_fopen_mode.c
    stdio/clearerr.c
    stdio/fclose.c
    stdio/feof.c
    stdio/ferror.c
    stdio/fgetc.c
    stdio/fgetpos.c
    stdio/fgets.c
    stdio/fopen.c
    stdio/fread.c
    stdio/freopen.c
    stdio/fscanf.c
    stdio/fseek.c
    stdio/fsetpos.c
    stdio/ftell.c
    stdio/printf.c
    stdio/putchar.c
    stdio/puts.c
    stdio/rewind.c
    stdio/setbuf.c
    stdio/setvbuf.c
    stdio/ungetc.c
    stdio/vfscanf.c
    stdio/vprintf.c

    string/strdup.c
    string/strndup.c)

add_library(kcrt "knostdlib" STATIC ${KERNEL_SOURCES})

target_include_directories(kcrt PUBLIC include)
target_compile_options(kcrt PRIVATE $<$<COMPILE_LANGUAGE:C,CXX>:-ffreestanding>)
