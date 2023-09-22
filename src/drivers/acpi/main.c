/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#include <acpip.h>
#include <amd64/port.h>
#include <crt_impl.h>
#include <ke.h>
#include <stdarg.h>
#include <string.h>
#include <vid.h>

extern AcpiObject *AcpipObjectTree;

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

static void PrintChar(char Data) {
    WritePortByte(0x402, Data);
}

static void PrintTabs(int Tabs) {
    while (Tabs--) {
        PrintChar(' ');
        PrintChar(' ');
        PrintChar(' ');
        PrintChar(' ');
    }
}

static void DumpTree(AcpiObject *Root, int Tabs) {
    PrintTabs(Tabs);
    PrintChar(Root->Name[0]);
    PrintChar(Root->Name[1]);
    PrintChar(Root->Name[2]);
    PrintChar(Root->Name[3]);
    PrintChar(0x0A);

    if (Root->Value.Type != ACPI_REGION && Root->Value.Type != ACPI_SCOPE &&
        Root->Value.Type != ACPI_DEVICE && Root->Value.Type != ACPI_PROCESSOR) {
        return;
    }

    for (AcpiObject *Leaf = Root->Value.Objects; Leaf; Leaf = Leaf->Next) {
        DumpTree(Leaf, Tabs + 1);
    }
}

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

    /* Dumping the ACPI namespace root. */
    DumpTree(AcpipObjectTree, 0);
}
