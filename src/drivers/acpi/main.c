/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#include <acpip.h>
#include <amd64/port.h>
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

static uint64_t EvaluateSta(AcpiObject *Root) {
    AcpipName RawName;
    RawName.LinkedObject = Root;
    RawName.Start = (const uint8_t *)"_STA";
    RawName.BacktrackCount = 0;
    RawName.SegmentCount = 1;

    AcpiObject *Sta = AcpipResolveObject(&RawName);
    if (!Sta) {
        return 0x0F;
    }

    AcpiValue MethodResult;
    AcpiValue *Value = &Sta->Value;
    if (Sta->Value.Type == ACPI_METHOD) {
        if (!AcpiExecuteMethod(Sta, 0, NULL, &MethodResult)) {
            return 0;
        }

        Value = &MethodResult;
    }

    uint64_t Result;
    return AcpipCastToInteger(Value, &Result, Value == &MethodResult) ? Result : 0;
}

static void EvaluateReg(AcpiObject *Root, AcpiObject *Region) {
    AcpipName RawName;
    RawName.LinkedObject = Root;
    RawName.Start = (const uint8_t *)"_REG";
    RawName.BacktrackCount = 0;
    RawName.SegmentCount = 1;

    AcpiValue Arguments[2];
    Arguments[0].Type = ACPI_INTEGER;
    Arguments[0].References = 1;
    Arguments[0].Integer = Region->Value.Region.RegionSpace;
    Arguments[1].Type = ACPI_INTEGER;
    Arguments[1].References = 1;
    Arguments[1].Integer = 1;

    AcpiExecuteMethod(AcpipResolveObject(&RawName), 2, Arguments, NULL);
}

static void EvaluateIni(AcpiObject *Root) {
    AcpipName RawName;
    RawName.LinkedObject = Root;
    RawName.Start = (const uint8_t *)"_INI";
    RawName.BacktrackCount = 0;
    RawName.SegmentCount = 1;
    AcpiExecuteMethod(AcpipResolveObject(&RawName), 0, NULL, NULL);
}

static void InitializeChildren(AcpiObject *Root) {
    for (AcpiObject *Device = Root->Value.Objects; Device != NULL; Device = Device->Next) {
        if (Device->Value.Type != ACPI_DEVICE) {
            continue;
        }

        uint64_t Sta = EvaluateSta(Device);
        if (Sta & 1) {
            EvaluateIni(Device);

            for (AcpiObject *Region = Device->Value.Objects; Region != NULL;
                 Region = Region->Next) {
                if (Region->Value.Type == ACPI_REGION) {
                    EvaluateReg(Device, Region);
                }
            }
        }

        if ((Sta & 1) | (Sta & 8)) {
            InitializeChildren(Device);
        }
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
    AcpipPopulateOverride();
    AcpipReadTables();

    /* Non standard, but call the Windows-specific global initialization method. */
    AcpiExecuteMethod(AcpiSearchObject("\\_INI"), 0, NULL, NULL);

    /* Call the _SB_ scope initialization method. */
    AcpiExecuteMethod(AcpiSearchObject("\\_SB_._INI"), 0, NULL, NULL);

    /* Initialize all devices inside the _SB_ scope. */
    InitializeChildren(AcpiSearchObject("\\_SB_"));

    /* Inform the ACPI BIOS that we're still using the classic PIC. */
    AcpiValue Argument;
    Argument.Type = ACPI_INTEGER;
    Argument.References = 1;
    Argument.Integer = 0;
    AcpiExecuteMethod(AcpiSearchObject("\\_PIC"), 1, &Argument, NULL);

    /* Let's start the shutdown process; Grab the FADT. */
    FadtHeader *Fadt = (FadtHeader *)AcpipFindTable("FACP");
    if (!Fadt) {
        printf("Could not get the FADT\n");
        return;
    }

    /* Try enabling ACPI (if its not already enabled). */
    if (Fadt->SmiCommandPort && Fadt->AcpiEnable && !(ReadPortWord(Fadt->Pm1aControlBlock) & 1)) {
        WritePortByte(Fadt->SmiCommandPort, Fadt->AcpiEnable);
        while (!(ReadPortWord(Fadt->Pm1aControlBlock) & 1))
            ;
    }

    /* Evaluate \\_S5_. */
    AcpiObject *S5Object = AcpiSearchObject("\\_S5_");
    AcpiValue S5MethodResult;
    AcpiValue *S5Value;
    if (!S5Object) {
        printf("This computer doesn't support S5 shutdown?\n");
        return;
    } else if (S5Object->Value.Type == ACPI_METHOD) {
        if (!AcpiExecuteMethod(S5Object, 0, NULL, &S5MethodResult)) {
            printf("Could not evaluate the S5 object\n");
            return;
        }

        S5Value = &S5MethodResult;
    } else {
        S5Value = &S5Object->Value;
    }

    /* Validate if we have a package with at least one integer. */
    AcpiValue *SlpTypA = &S5Value->Package.Data[0].Value;
    AcpiValue *SlpTypB = &S5Value->Package.Data[1].Value;
    if (S5Value->Type != ACPI_PACKAGE) {
        printf("S5 isn't a package\n");
        return;
    } else if (S5Value->Package.Size < 2) {
        printf("S5 has less than 2 items\n");
        return;
    } else if (
        !S5Value->Package.Data[0].Type || SlpTypA->Type != ACPI_INTEGER ||
        !S5Value->Package.Data[1].Type || SlpTypB->Type != ACPI_INTEGER) {
        printf("One of the two required S5 values aren't an integer\n");
        return;
    }

    /* Execute _PTS and _GTS if they exist. */
    Argument.Integer = 5;
    AcpiExecuteMethod(AcpiSearchObject("\\_PTS"), 1, &Argument, NULL);
    AcpiExecuteMethod(AcpiSearchObject("\\_GTS"), 1, &Argument, NULL);

    /* Enter S5/shutdown. */
    WritePortWord(Fadt->Pm1aControlBlock, (SlpTypA->Integer << 10) | (1 << 13));
    if (Fadt->Pm1bControlBlock) {
        WritePortWord(Fadt->Pm1bControlBlock, (SlpTypB->Integer << 10) | (1 << 13));
    }

    while (1)
        ;
}
