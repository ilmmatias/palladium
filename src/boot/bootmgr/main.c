/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#include <boot.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

int __vprintf(
    const char *format,
    va_list vlist,
    void *context,
    void (*put_buf)(const void *buffer, int size, void *context));

void put_buf(const void *buffer, int size, void *context) {
    (void)context;
    const char *src = buffer;
    while (size--) {
        BiPutChar(*(src++));
    }
}

int printf(const char *format, ...) {
    va_list vlist;
    va_start(vlist, format);
    int size = __vprintf(format, vlist, NULL, put_buf);
    va_end(vlist);
    return size;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function is the BOOTMGR architecture-independent entry point.
 *     We detect and initialize all required hardware, show the boot menu, load up the OS, and
 *     transfer control to it.
 *
 * PARAMETERS:
 *     BootBlock - Information obtained while setting up the boot environment.
 *
 * RETURN VALUE:
 *     Does not return.
 *-----------------------------------------------------------------------------------------------*/
[[noreturn]] void BmMain(void *BootBlock) {
    BmInitDisplay();
    BmInitArch();
    BmInitMemory(BootBlock);

    char TestString[50];
    sprintf(TestString, "Hello, World! Here is a number :0x%-*.*x:", 16, 8, 0xC0DE);
    printf("%s\n", TestString);

    while (1)
        ;
}
