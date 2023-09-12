/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#include <acpip.h>
#include <ctype.h>
#include <stddef.h>
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
 *     This function frees the inside of a term arg/value.
 *
 * PARAMETERS:
 *     Value - Value returned by AcpipExecuteOpcode.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void AcpiFreeValueData(AcpiValue *Value) {
    switch (Value->Type) {
        case ACPI_STRING:
            free(Value->String);
            break;

        case ACPI_BUFFER:
            /* NULL buffers are allowed for 0-sized data. */
            if (Value->Buffer.Data) {
                free(Value->Buffer.Data);
            }

            break;

        case ACPI_PACKAGE:
            for (uint8_t i = 0; i < Value->Package.Size; i++) {
                if (Value->Package.Data[i].Type) {
                    AcpiFreeValueData(&Value->Package.Data[i].Value);
                }
            }

            free(Value->Package.Data);
            break;

        /* Anything else really shouldn't be freed at all, as it is either integers, something
           that doesn't need to be freed, or something that isn't a term arg. */
        default:
            break;
    }
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function frees an entire term arg/value, including the AcpiValue pointer itself.
 *
 * PARAMETERS:
 *     Value - Value returned by AcpipExecuteOpcode; It will be invalid after this call.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void AcpiFreeValue(AcpiValue *Value) {
    AcpiFreeValueData(Value);
    free(Value);
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function tries casting the result of a previous ExecuteOpcode into an integer.
 *
 * PARAMETERS:
 *     Value - Result of ExecuteOpcode.
 *     Result - Output as an integer.
 *
 * RETURN VALUE:
 *     1 on success, 0 otherwise.
 *-----------------------------------------------------------------------------------------------*/
int AcpipCastToInteger(AcpiValue *Value, uint64_t *Result) {
    if (Value->Type == ACPI_INTEGER) {
        *Result = Value->Integer;
        return 1;
    }

    *Result = 0;
    switch (Value->Type) {
        /* The contents of the buffer should be copied 1-to-1 into the integer, making sure to
           validate the size of the buffer. */
        case ACPI_BUFFER:
            for (uint64_t i = 0; i < (Value->Buffer.Size > 8 ? 8 : Value->Buffer.Size); i++) {
                *Result |= Value->Buffer.Data[i] << (i * 8);
            }

            break;

        /* The string is copied assuming every position is a hex-character; We have strtoull, which
           does the conversion for us. */
        case ACPI_STRING:
            *Result = strtoull(Value->String, NULL, 16);
            break;

        default:
            return 0;
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
 *
 * RETURN VALUE:
 *     1 on success, 0 otherwise.
 *-----------------------------------------------------------------------------------------------*/
int AcpipCastToString(AcpiValue *Value, int ImplicitCast) {
    if (Value->Type == ACPI_STRING) {
        return 1;
    }

    char *String;
    switch (Value->Type) {
        /* Integers get converted using snprintf into 16 hex digits. */
        case ACPI_INTEGER: {
            String = malloc(17);
            if (!String) {
                return 0;
            }

            if (snprintf(String, 17, "%016llX", Value->Integer) != 16) {
                free(String);
                return 0;
            }

            break;
        }

        /* Buffers should be converted into a list of either space or comma separated pairs of 2
           hex chars (prefixed with 0x); We use space for implicit conversion, and comma for
           explicit. */
        case ACPI_BUFFER: {
            String = malloc(Value->Buffer.Size * 5);
            if (!String) {
                return 0;
            }

            for (uint64_t i = 0; i < Value->Buffer.Size; i++) {
                int PrintfSize = 5;
                const char *PrintfFormat = "0x%02hhX";

                if (i < Value->Buffer.Size - 1) {
                    PrintfSize = 6;
                    PrintfFormat = ImplicitCast ? "0x%02hhX " : "0x%02hhX,";
                }

                if (snprintf(
                        Value->String + i * 5, PrintfSize, PrintfFormat, Value->Buffer.Data[i]) !=
                    PrintfSize - 1) {
                    free(String);
                    return 0;
                }
            }

            break;
        }

        /* For everything else, we'll just be converting their type into a string, and returning
           that. */
        default: {
            String = strdup(Types[Value->Type]);
            if (!String) {
                return 0;
            }

            break;
        }
    }

    AcpiFreeValueData(Value);
    Value->Type = ACPI_STRING;
    Value->String = String;

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
    uint8_t *Buffer = NULL;

    switch (Value->Type) {
        /* Integers get their underlying in-memory representation copied as an 8-byte buffer. */
        case ACPI_INTEGER: {
            BufferSize = 16;
            Buffer = malloc(16);

            if (!Buffer) {
                return 0;
            }

            *((uint64_t *)Buffer) = Value->Integer;
            break;
        }

        /* Strings mostly pass through, except for 0-length, they just become 0-sized buffers
           (instead of length==1). */
        case ACPI_STRING: {
            size_t StringSize = strlen(Value->String);

            if (StringSize) {
                BufferSize = StringSize + 1;
                Buffer = (uint8_t *)strdup(Value->String);
                if (!Buffer) {
                    return 0;
                }
            }

            break;
        }

        default:
            return 0;
    }

    AcpiFreeValueData(Value);
    Value->Type = ACPI_BUFFER;
    Value->Buffer.Size = BufferSize;
    Value->Buffer.Data = Buffer;

    return 1;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function tries executing the next termarg in the scope.
 *
 * PARAMETERS:
 *     State - AML state containing the current scope.
 *     Value - Output as an integer.
 *
 * RETURN VALUE:
 *     1 on success, 0 otherwise.
 *-----------------------------------------------------------------------------------------------*/
int AcpipExecuteInteger(AcpipState *State, uint64_t *Result) {
    AcpiValue Value;
    if (!AcpipExecuteOpcode(State, &Value)) {
        return 0;
    }

    int ReturnValue = AcpipCastToInteger(&Value, Result);
    AcpiFreeValueData(&Value);

    return ReturnValue;
}
