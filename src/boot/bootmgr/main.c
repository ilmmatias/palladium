/* SPDX-FileCopyrightText: (C) 2023 yuuma03
 * SPDX-License-Identifier: BSD-3-Clause */

#include <boot.h>
#include <stdarg.h>
#include <stdlib.h>

size_t __vprintf(
    const char *format,
    va_list vlist,
    void *context,
    void (*put_buf)(const void *buffer, size_t size, void *context));

void put_buf(const void *buffer, size_t size, void *context) {
    (void)context;
    const char *src = buffer;
    while (size--) {
        BiPutChar(*(src++));
    }
}

size_t printf(const char *format, ...) {
    va_list vlist;
    va_start(vlist, format);
    size_t size = __vprintf(format, vlist, NULL, put_buf);
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

    const char *p = "10 200000000000000000000000000000 30 -40 - 42";
    printf("Parsing '%s':\n", p);
    char *end = NULL;
    for (unsigned long i = strtoul(p, &end, 10); p != end; i = strtoul(p, &end, 10)) {
        printf("'%.*s' -> ", (int)(end - p), p);
        p = end;
        printf("%lu\n", i);
    }
    printf("After the loop p points to '%s'\n", p);

    while (1)
        ;
}
