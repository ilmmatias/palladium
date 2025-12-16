/* SPDX-FileCopyrightText: (C) 2023-2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <acpip.h>
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
 *     true/false depending on success.
 *-----------------------------------------------------------------------------------------------*/
bool AcpipCastToInteger(AcpiValue *Value, uint64_t *Result, bool Cleanup) {
    if (Value->Type == ACPI_INTEGER) {
        *Result = Value->Integer;

        if (Cleanup) {
            AcpiRemoveReference(Value, false);
        }

        return true;
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

        /* Field units need to be read first, which gives us either a buffer (which we can treat as
         * any ACPI_BUFFER), or an integer (which we don't need to do anything special). */
        case ACPI_FIELD_UNIT: {
            AcpiValue Field;
            if (!AcpipReadField(Value, &Field)) {
                if (Cleanup) {
                    AcpiRemoveReference(Value, false);
                }

                return false;
            }

            if (Field.Type == ACPI_INTEGER) {
                *Result = Field.Integer;
            } else {
                for (uint64_t i = 0; i < (Field.Buffer->Size > 8 ? 8 : Field.Buffer->Size); i++) {
                    *Result |= (uint64_t)Field.Buffer->Data[i] << (i * 8);
                }
            }

            AcpiRemoveReference(&Field, false);
            break;
        }

        default:
            if (Cleanup) {
                AcpiRemoveReference(Value, false);
            }

            return false;
    }

    if (Cleanup) {
        AcpiRemoveReference(Value, false);
    }

    return true;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function mounts the printf format string for the current buffer position.
 *
 * PARAMETERS:
 *     Position - Which index we're currently printing.
 *     Size - Size of the buffer in elements.
 *     ImplicitCast - Set this to 0 if you only want explicit casts, or to 1 otherwise.
 *     Decimal - Set this to 0 if you want decimal digits instead of hexadecimal, or to 1
 *               otherwise.
 *
 * RETURN VALUE:
 *     The printf format string.
 *-----------------------------------------------------------------------------------------------*/
static const char *
GetBufferStringFormat(uint64_t Position, uint64_t Size, bool ImplicitCast, bool Decimal) {
    if (Decimal) {
        if (Position < Size - 1) {
            return ImplicitCast ? "%ld " : "%ld,";
        } else {
            return "%ld";
        }
    } else {
        if (Position < Size - 1) {
            return ImplicitCast ? "0x%02hhX " : "0x%02hhX,";
        } else {
            return "0x%02hhX";
        }
    }
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
 *     true/false depending on success.
 *-----------------------------------------------------------------------------------------------*/
bool AcpipCastToString(AcpiValue *Value, bool ImplicitCast, bool Decimal) {
    if (Value->Type == ACPI_STRING) {
        return true;
    }

    AcpiString *String;
    switch (Value->Type) {
        /* Integers have two conversion paths:
               For hexadecimal, they get converted using snprintf into 16 hex digits.
               For decimal, we need snprintf to get the output size, then we convert it. */
        case ACPI_INTEGER: {
            if (Decimal) {
                int Size = snprintf(NULL, 0, "%lld", Value->Integer);
                String = AcpipAllocateBlock(sizeof(AcpiString) + Size + 1);
                if (!String) {
                    return false;
                }

                if (snprintf(String->Data, Size + 1, "%lld", Value->Integer) != Size) {
                    AcpipFreeBlock(String);
                    return false;
                }
            } else {
                String = AcpipAllocateBlock(sizeof(AcpiString) + 17);
                if (!String) {
                    return false;
                }

                if (snprintf(String->Data, 17, "%016llX", Value->Integer) != 16) {
                    AcpipFreeBlock(String);
                    return false;
                }
            }

            break;
        }

        /* Buffers should be converted into a list of either space or comma separated pairs of
           digits; We use space for implicit conversion, and comma for explicit. */
        case ACPI_BUFFER: {
            int Size = 0;

            for (uint64_t i = 0; i < Value->Buffer->Size; i++) {
                const char *PrintfFormat =
                    GetBufferStringFormat(i, Value->Buffer->Size, ImplicitCast, Decimal);
                Size += snprintf(NULL, 0, PrintfFormat, Value->Buffer->Data[i]);
            }

            String = AcpipAllocateBlock(sizeof(AcpiString) + Size + 1);
            if (!String) {
                return false;
            }

            int Offset = 0;
            for (uint64_t i = 0; i < Value->Buffer->Size; i++) {
                const char *PrintfFormat =
                    GetBufferStringFormat(i, Value->Buffer->Size, ImplicitCast, Decimal);
                Offset += snprintf(
                    String->Data + Offset, Size - Offset + 1, PrintfFormat, Value->Buffer->Data[i]);
            }

            break;
        }

        /* For everything else, we'll just be converting their type into a string, and returning
           that. */
        default: {
            String = AcpipAllocateBlock(sizeof(AcpiString) + strlen(Types[Value->Type]) + 1);
            if (!String) {
                return false;
            }

            strcpy(String->Data, Types[Value->Type]);
            break;
        }
    }

    AcpiRemoveReference(Value, false);
    Value->Type = ACPI_STRING;
    Value->String = String;
    Value->String->References = 1;

    return true;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function tries casting the result of a previous ExecuteOpcode into a buffer.
 *
 * PARAMETERS:
 *     Value - I/O; Result of ExecuteOpcode; Will become the converted buffer after the cast.
 *
 * RETURN VALUE:
 *     true/false depending on success.
 *-----------------------------------------------------------------------------------------------*/
bool AcpipCastToBuffer(AcpiValue *Value) {
    if (Value->Type == ACPI_BUFFER) {
        return true;
    }

    uint64_t BufferSize = 0;
    AcpiBuffer *Buffer;

    switch (Value->Type) {
        /* Integers get their underlying in-memory representation copied as an 8-byte buffer. */
        case ACPI_INTEGER: {
            BufferSize = 16;
            Buffer = AcpipAllocateBlock(sizeof(AcpiBuffer) + 16);
            if (!Buffer) {
                AcpiRemoveReference(Value, false);
                return false;
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
                AcpiRemoveReference(Value, false);
                return false;
            }

            if (BufferSize) {
                strcpy((char *)Buffer->Data, Value->String->Data);
            }

            break;
        }

        /* Field units can be integers or buffers (we need the same logic as ACPI_INTEGER for the
         * earlier, and we pass-through for the later). */
        case ACPI_FIELD_UNIT: {
            AcpiValue Field;
            if (!AcpipReadField(Value, &Field)) {
                AcpiRemoveReference(Value, false);
                return false;
            }

            if (Field.Type != ACPI_INTEGER) {
                AcpiRemoveReference(Value, false);
                memcpy(Value, &Field, sizeof(AcpiValue));
                return true;
            }

            BufferSize = 16;
            Buffer = AcpipAllocateBlock(sizeof(AcpiBuffer) + 16);
            if (!Buffer) {
                AcpiRemoveReference(Value, false);
                return false;
            }

            *((uint64_t *)Buffer->Data) = Field.Integer;
            break;
        }

        default:
            AcpiRemoveReference(Value, false);
            return false;
    }

    AcpiRemoveReference(Value, false);
    Value->Type = ACPI_BUFFER;
    Value->References = 1;
    Value->Buffer = Buffer;
    Value->Buffer->References = 1;
    Value->Buffer->Size = BufferSize;

    return true;
}
