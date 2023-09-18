/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#include <acpip.h>
#include <crt_impl.h>
#include <ke.h>
#include <stdarg.h>
#include <string.h>
#include <vid.h>

/* stdio START; for debugging our AML interpreter. */
static void put_buf(const void *buffer, int size, void *context) {
    (void)context;
    for (int i = 0; i < size; i++) {
        VidPutChar(((const char *)buffer)[i]);
    }
}

int printf(const char *format, ...) {
    va_list vlist;
    va_start(vlist, format);
    int len = __vprintf(format, vlist, NULL, put_buf);
    va_end(vlist);
    return len;
}
/* stdio END */

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function is the entry point of the ACPI compatibility module. We're responsible for
 *     getting things ready so that other drivers can use ACPI functions (such as routing PCI
 *     IRQs).
 *
 * PARAMETERS:
 *     None.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void DriverEntry(void) {
    AcpipPopulatePredefined();

    if (KiGetAcpiTableType() == KI_ACPI_RDST) {
        AcpipInitializeFromRsdt();
    } else {
        AcpipInitializeFromXsdt();
    }

    AcpiObject *Method = AcpiSearchObject("\\_SB_.HPET._STA");
    if (!Method) {
        printf("The \\_SB_.HPET._STA objects doesn't seem to exist\n");
        return;
    }

    printf("%d\n", AcpiExecuteMethod(Method, 0, NULL, NULL));
}
