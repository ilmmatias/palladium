/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <acpip.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *Types[] = {
    "[Uninitialized Object]",
    "[Integer]",
    "[String]",
    "[Buffer]",
    "[Package]",
    "[Field]",
    "[Device]",
    "[Event]",
    "[Control Method]",
    "[Mutex]",
    "[Operation Region]",
    "[Power Resource]",
    "[Processor]",
    "[Thermal Zone]",
    "[Buffer Field]",
    "[Reserved]",
    "[Debug Object]",
    "[Scope]",
};

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function tries casting the result of a previous ExecuteOpcode into an integer.
 *     This will discard the `Value` automatically on success.
 *
 * PARAMETERS:
 *     Value - Result of ExecuteOpcode.
 *             This is assumed to be stack allocated.
 *     Result - Output as an integer.
 *     Cleanup - Set this to one if we should RemoveReference buffers/strings.
 *
 * RETURN VALUE:
 *     1 on success, 0 otherwise.
 *-----------------------------------------------------------------------------------------------*/
int AcpipCastToInteger(AcpiValue *Value, uint64_t *Result, int Cleanup) {
    if (Value->Type == ACPI_INTEGER) {
        *Result = Value->Integer;

        if (Cleanup) {
            AcpiRemoveReference(Value, 0);
        }

        return 1;
    }

    *Result = 0;
    switch (Value->Type) {
        /* The contents of the buffer should be copied 1-to-1 into the integer, making sure to
           validate the size of the buffer. */
        case ACPI_BUFFER:
            for (uint64_t i = 0; i < (Value->Buffer->Size > 8 ? 8 : Value->Buffer->Size); i++) {
                *Result |= Value->Buffer->Data[i] << (i * 8);
            }

            break;

        /* The string is copied assuming every position is a hex-character; We have strtoull, which
           does the conversion for us. */
        case ACPI_STRING:
            *Result = strtoull(Value->String->Data, NULL, 16);
            break;

        default:
            if (Cleanup) {
                AcpiRemoveReference(Value, 0);
            }

            return 0;
    }

    if (Cleanup) {
        AcpiRemoveReference(Value, 0);
    }

    return 1;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function tries casting the result of a previous ExecuteOpcode into a string.
 *
 * PARAMETERS:
 *     Value - I/O; Result of ExecuteOpcode; Will become the converted string after the cast.
 *     ImplicitCast - Set this to 0 if you only want explicit casts, or to 1 otherwise.
 *     Decimal - Set this to 0 if you want decimal digits instead of hexadecimal, or to 1
 *               otherwise.
 *
 * RETURN VALUE:
 *     1 on success, 0 otherwise.
 *-----------------------------------------------------------------------------------------------*/
int AcpipCastToString(AcpiValue *Value, int ImplicitCast, int Decimal) {
    if (Value->Type == ACPI_STRING) {
        return 1;
    }

    AcpiString *String;
    switch (Value->Type) {
        /* Integers have two conversion paths:
               For hexadecimal, they get converted using snprintf into 16 hex digits.
               For decimal, we need snprintf to get the output size, then we convert it. */
        case ACPI_INTEGER: {
            if (Decimal) {
                int Size = snprintf(NULL, 0, "%ld", Value->Integer);
                String = AcpipAllocateBlock(sizeof(AcpiString) + Size + 1);
                if (!String) {
                    return 0;
                }

                if (snprintf(String->Data, Size + 1, "%ld", Value->Integer) != Size) {
                    AcpipFreeBlock(String);
                    return 0;
                }
            } else {
                String = AcpipAllocateBlock(sizeof(AcpiString) + 17);
                if (!String) {
                    return 0;
                }

                if (snprintf(String->Data, 17, "%016lX", Value->Integer) != 16) {
                    AcpipFreeBlock(String);
                    return 0;
                }
            }

            break;
        }

        /* Buffers should be converted into a list of either space or comma separated pairs of
           digits; We use space for implicit conversion, and comma for explicit. */
        case ACPI_BUFFER: {
            /* Hexadecimal is the easier case, as we know we should always have 4 digits per
               buffer item; For decimal, we're not sure of the size until we run snprintf, so
               we need two loops using snprintf. */
            if (Decimal) {
                int Size = 0;

                for (uint64_t i = 0; i < Value->Buffer->Size; i++) {
                    const char *PrintfFormat = "%ld";

                    if (i < Value->Buffer->Size - 1) {
                        PrintfFormat = ImplicitCast ? "%ld " : "%ld,";
                    }

                    Size += snprintf(NULL, 0, PrintfFormat, Value->Buffer->Data[i]);
                }

                String = AcpipAllocateBlock(sizeof(AcpiString) + Size + 1);
                if (!String) {
                    return 0;
                }

                int Offset = 0;
                for (uint64_t i = 0; i < Value->Buffer->Size; i++) {
                    const char *PrintfFormat = "%ld";

                    if (i < Value->Buffer->Size - 1) {
                        PrintfFormat = ImplicitCast ? "%ld " : "%ld,";
                    }

                    Offset += snprintf(
                        String->Data + Offset,
                        Size - Offset + 1,
                        PrintfFormat,
                        Value->Buffer->Data[i]);
                }
            } else {
                String = AcpipAllocateBlock(sizeof(AcpiString) + Value->Buffer->Size * 5);
                if (!String) {
                    return 0;
                }

                for (uint64_t i = 0; i < Value->Buffer->Size; i++) {
                    int PrintfSize = 5;
                    const char *PrintfFormat = "0x%02hhX";

                    if (i < Value->Buffer->Size - 1) {
                        PrintfSize = 6;
                        PrintfFormat = ImplicitCast ? "0x%02hhX " : "0x%02hhX,";
                    }

                    if (snprintf(
                            String->Data + i * 5,
                            PrintfSize,
                            PrintfFormat,
                            Value->Buffer->Data[i]) != PrintfSize - 1) {
                        AcpipFreeBlock(String);
                        return 0;
                    }
                }
            }

            break;
        }

        /* For everything else, we'll just be converting their type into a string, and returning
           that. */
        default: {
            String = AcpipAllocateBlock(sizeof(AcpiString) + strlen(Types[Value->Type]) + 1);
            if (!String) {
                return 0;
            }

            strcpy(String->Data, Types[Value->Type]);
            break;
        }
    }

    AcpiRemoveReference(Value, 0);
    Value->Type = ACPI_STRING;
    Value->String = String;
    Value->String->References = 1;

    return 1;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function tries casting the result of a previous ExecuteOpcode into a buffer.
 *
 * PARAMETERS:
 *     Value - I/O; Result of ExecuteOpcode; Will become the converted buffer after the cast.
 *
 * RETURN VALUE:
 *     1 on success, 0 otherwise.
 *-----------------------------------------------------------------------------------------------*/
int AcpipCastToBuffer(AcpiValue *Value) {
    if (Value->Type == ACPI_BUFFER) {
        return 1;
    }

    uint64_t BufferSize = 0;
    AcpiBuffer *Buffer;

    switch (Value->Type) {
        /* Integers get their underlying in-memory representation copied as an 8-byte buffer. */
        case ACPI_INTEGER: {
            BufferSize = 16;
            Buffer = AcpipAllocateBlock(sizeof(AcpiBuffer) + 16);
            if (!Buffer) {
                AcpiRemoveReference(Value, 0);
                return 0;
            }

            *((uint64_t *)Buffer->Data) = Value->Integer;
            break;
        }

        /* Strings mostly pass through, except for 0-length, they just become 0-sized buffers
           (instead of length==1). */
        case ACPI_STRING: {
            BufferSize = strlen(Value->String->Data);
            Buffer = AcpipAllocateBlock(sizeof(AcpiBuffer) + (BufferSize ? ++BufferSize : 0));
            if (!Buffer) {
                AcpiRemoveReference(Value, 0);
                return 0;
            }

            if (BufferSize) {
                strcpy((char *)Buffer->Data, Value->String->Data);
            }

            break;
        }

        default:
            AcpiRemoveReference(Value, 0);
            return 0;
    }

    AcpiRemoveReference(Value, 0);
    Value->Type = ACPI_BUFFER;
    Value->Buffer->Size = BufferSize;

    return 1;
}
