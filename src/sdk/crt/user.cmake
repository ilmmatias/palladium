# SPDX-FileCopyrightText: (C) 2023-2025 ilmmatias
# SPDX-License-Identifier: GPL-3.0-or-later

set(USER_SOURCES
    ${SOURCES}

    os/__acquire_lock.c
    os/__allocate_pages.c
    os/__close_file.c
    os/__create_lock.c
    os/__delete_lock.c
    os/__open_file.c
    os/__read_file.c
    os/__release_lock.c
    os/__seek_file.c
    os/__tell_file.c
    os/__terminate_process.c
    os/__write_file.c
    os/handles.c
    os/startup.c

    stdio/__parse_open_mode.c
    stdio/clearerr.c
    stdio/fclose.c
    stdio/feof.c
    stdio/ferror.c
    stdio/fflush.c
    stdio/fgetc.c
    stdio/fgetpos.c
    stdio/fgets.c
    stdio/flockfile.c
    stdio/fopen.c
    stdio/fprintf.c
    stdio/fputc.c
    stdio/fputs.c
    stdio/fread.c
    stdio/freopen.c
    stdio/fscanf.c
    stdio/fseek.c
    stdio/fsetpos.c
    stdio/ftell.c
    stdio/funlockfile.c
    stdio/fwrite.c
    stdio/getchar.c
    stdio/gets.c
    stdio/printf.c
    stdio/putchar.c
    stdio/puts.c
    stdio/rewind.c
    stdio/scanf.c
    stdio/setbuf.c
    stdio/setvbuf.c
    stdio/ungetc.c
    stdio/vfprintf.c
    stdio/vfscanf.c
    stdio/vprintf.c
    stdio/vscanf.c

    stdlib/allocator.c

    string/strdup.c
    string/strndup.c)

set(USER_EXE_STARTUP_SOURCES
    os/crtbegin.c
    os/crtend.c
    os/execrt0.c)

set(USER_DLL_STARTUP_SOURCES
    os/crtbegin.c
    os/crtend.c
    os/dllcrt0.c)

add_library(ucrt "osapi" SHARED ${USER_SOURCES} ucrt.def)
target_include_directories(ucrt PUBLIC include)
target_compile_options(ucrt PRIVATE $<$<COMPILE_LANGUAGE:C>:-ffreestanding>)
target_link_options(ucrt PRIVATE -Wl,--entry=CrtDllStartup)

add_library(ucrt_exe_startup "nostdlib" OBJECT ${USER_EXE_STARTUP_SOURCES})
target_compile_options(ucrt_exe_startup PRIVATE $<$<COMPILE_LANGUAGE:C>:-ffreestanding>)

add_library(ucrt_dll_startup "nostdlib" OBJECT ${USER_DLL_STARTUP_SOURCES})
target_compile_options(ucrt_dll_startup PRIVATE $<$<COMPILE_LANGUAGE:C>:-ffreestanding>)
