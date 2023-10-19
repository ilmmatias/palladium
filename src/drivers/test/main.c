/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#include <acpi.h>
#include <crt_impl.h>
#include <stdarg.h>
#include <vid.h>

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     Wrapper around VidPutChar for __vprint.
 *
 * PARAMETERS:
 *     Buffer - What we need to display.
 *     Size - Size of that data; The data is not required to be NULL terminated, so this need to be
 *            taken into account!
 *     Context - Always NULL for us.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
static void PutBuffer(const void *buffer, int size, void *context) {
    (void)context;
    for (int i = 0; i < size; i++) {
        VidPutChar(((const char *)buffer)[i]);
    }
}

static int printf(const char *fmt, ...) {
    va_list vlist;
    va_start(vlist, fmt);
    int size = __vprintf(fmt, vlist, NULL, PutBuffer);
    va_end(vlist);
    return size;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function is the entry point of the test driver; We're just used to see if the boot
 *     manager is properly importing functions from other drivers.
 *
 * PARAMETERS:
 *     None.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void DriverEntry(void) {
    printf("Hello, World!\n");
    printf(
        "Here is the result of AcpiSearchObject(NULL, \"_S5_\"): %p\n",
        AcpiSearchObject(NULL, "_S5_"));
}
