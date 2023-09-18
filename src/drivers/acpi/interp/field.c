/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#include <acpip.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function reads data from the given region, using the given offset and the offset of
 *     the region field itself.
 *
 * PARAMETERS:
 *     Source - AML value containing a region field.
 *     Offset - Offset within the region field; Expected to be a byte offset, so shift it right
 *              by 3 first!
 *
 * RETURN VALUE:
 *     Requested data.
 *-----------------------------------------------------------------------------------------------*/
static uint64_t ReadRegionField(AcpiValue *Source, int Offset) {
    printf("ReadRegionField(%p, %u)\n", Source, Offset);
    while (1)
        ;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function writes data into the given region, using the given offset and the offset of
 *     the region field itself.
 *
 * PARAMETERS:
 *     Source - AML value containing a region field.
 *     Offset - Offset within the region field; Expected to be a byte offset, so shift it right
 *              by 3 first!
 *     Data - What data to store in the offset.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
static void WriteRegionField(AcpiValue *Source, int Offset, uint64_t Data) {
    printf("WriteRegionField(%p, %u, 0x%016llx)\n", Source, Offset, Data);
    while (1)
        ;
}

int AcpipReadField(AcpiValue *Source, AcpiValue *Target) {
    /* We need to check for access width > 1 byte; Any invalid value will also be assumed to be 1
       byte. */
    int AccessWidth = 8;
    switch (Source->FieldUnit.AccessType & 0x0F) {
        case 2:
            AccessWidth = 16;
            break;
        case 3:
            AccessWidth = 32;
            break;
        case 4:
            AccessWidth = 64;
            break;
    }

    /* Anything over 64-bits doesn't fit inside an integer; Otherwise, we don't need to allocate
       any extra memory.
       Index fields are always equal to 64 bits (the access width instead says )*/
    uint64_t *Buffer;
    if (Source->FieldUnit.Length > 64 && Source->FieldUnit.FieldType != ACPI_INDEX_FIELD) {
        Target->Type = ACPI_BUFFER;
        Target->Buffer.Size = (Source->FieldUnit.Length + 63) >> 3;
        Target->Buffer.Data = malloc(Target->Buffer.Size);

        if (!Target->Buffer.Data) {
            return 0;
        }

        memset(Target->Buffer.Data, 0, (Source->FieldUnit.Length + 63) >> 3);
        Buffer = (uint64_t *)Target->Buffer.Data;
    } else {
        Target->Type = ACPI_INTEGER;
        Target->Integer = 0;
        Buffer = &Target->Integer;
    }

    /* We need to respect the access width, and as such, read item by item from the region,
       merging them into the buffer as we proceed. */
    int ItemCount = (Source->FieldUnit.Length + AccessWidth - 1) / AccessWidth;
    uint64_t Item = 0;
    for (int i = 0; i < ItemCount; i++) {
        int Offset = (AccessWidth >> 3) * i;
        uint64_t Value;

        switch (Source->FieldUnit.FieldType) {
            case ACPI_FIELD:
                Value = ReadRegionField(Source, Offset);
                break;

            case ACPI_INDEX_FIELD:
                /* Index field means we need to write into the index location, followed by R/W'ing
                   from/into the data location; The given offset inside the index field is in bits,
                   but it needs to be converted into the granularity that the DefIndexField op gave
                   us. */

                WriteRegionField(
                    &Source->FieldUnit.Region->Value,
                    0,
                    Offset + ((Source->FieldUnit.Offset / AccessWidth) >> 3));

                Value = ReadRegionField(&Source->FieldUnit.Data->Value, 0);
                break;
        }

        /* Unaligned start, if we have more data, save into a temporary variable, as we need to
           merge with the next item. */
        if (!i && (Source->FieldUnit.Offset & 7)) {
            Value >>= Source->FieldUnit.Offset & 7;

            if (i + 1 < ItemCount) {
                Item = Value;
                continue;
            }
        }

        if (i == 1 && (Source->FieldUnit.Offset & 7)) {
            Value = (Value << (8 - (Source->FieldUnit.Offset & 7))) | Item;
        }

        *(Buffer++) = Value;
    }

    return 1;
}
