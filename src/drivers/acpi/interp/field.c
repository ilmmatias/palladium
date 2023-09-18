/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#include <acpip.h>
#include <mm.h>
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
 *     Size - How many bytes we should read; It should be the size of a Byte/Word/DWord/QWord.
 *            If its not, we'll assume it's a byte.
 *
 * RETURN VALUE:
 *     Requested data.
 *-----------------------------------------------------------------------------------------------*/
static uint64_t ReadRegionField(AcpiValue *Source, int Offset, int Size) {
    switch (Source->Region.RegionSpace) {
        /* SystemMemory; We'll be reading a physical memory address. */
        case 0: {
            void *Address = MI_PADDR_TO_VADDR(Source->Region.RegionOffset + Offset);
            return Size == 2   ? *(uint16_t *)Address
                   : Size == 4 ? *(uint32_t *)Address
                   : Size == 8 ? *(uint64_t *)Address
                               : *(uint8_t *)Address;
        }

        default:
            printf(
                "ReadRegionField(%p, %u, %u), RegionSpace = %hhu\n",
                Source,
                Offset,
                Size,
                Source->Region.RegionSpace);
            while (1)
                ;
    }
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
 *     Size - How many bytes we should write; Any value other than <1 or >8 will be assumed to be
 *            1.
 *     Data - What data to store in the offset.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
static void WriteRegionField(AcpiValue *Source, int Offset, int Size, uint64_t Data) {
    printf("WriteRegionField(%p, %u, %u, %llu)\n", Source, Offset, Size, Data);
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
       any extra memory. */
    uint8_t *Buffer;
    if (Source->FieldUnit.Length > 64) {
        Target->Type = ACPI_BUFFER;
        Target->Buffer.Size = (Source->FieldUnit.Length + AccessWidth - 1) >> 3;
        Target->Buffer.Data = malloc(Target->Buffer.Size);

        if (!Target->Buffer.Data) {
            return 0;
        }

        memset(Target->Buffer.Data, 0, (Source->FieldUnit.Length + AccessWidth - 1) >> 3);
        Buffer = Target->Buffer.Data;
    } else {
        Target->Type = ACPI_INTEGER;
        Target->Integer = 0;
        Buffer = (uint8_t *)&Target->Integer;
    }

    /* We need to respect the access width, and as such, read item by item from the region,
       merging them into the buffer as we proceed. */

    int UnalignedOffset = Source->FieldUnit.Offset & (AccessWidth - 1);
    int ItemCount = (Source->FieldUnit.Length + UnalignedOffset + AccessWidth - 1) / AccessWidth;

    uint64_t Item = 0;

    for (int i = 0; i < ItemCount; i++) {
        int Offset = (Source->FieldUnit.Offset >> 3) + (AccessWidth >> 3) * i;
        uint64_t Value;

        switch (Source->FieldUnit.FieldType) {
            case ACPI_FIELD:
                Value = ReadRegionField(&Source->FieldUnit.Region->Value, Offset, AccessWidth >> 3);
                break;

            case ACPI_INDEX_FIELD:
                /* Index field means we need to write into the index location, followed by R/W'ing
                   from/into the data location; The given offset inside the index field is in bits,
                   but it needs to be converted into the granularity that the DefIndexField op gave
                   us. */

                WriteRegionField(&Source->FieldUnit.Region->Value, 0, Offset, sizeof(uint64_t));
                Value = ReadRegionField(&Source->FieldUnit.Data->Value, 0, AccessWidth >> 3);

                break;
        }

        /* Unaligned start, if we have more data, save into a temporary variable, as we need to
           merge with the next item. */
        if (!i && UnalignedOffset) {
            Value >>= Source->FieldUnit.Offset & 7;

            if (i + 1 < ItemCount) {
                Item = Value;
                continue;
            }
        }

        if (i == 1 && UnalignedOffset) {
            Value = (Value << (AccessWidth - UnalignedOffset)) | Item;
        }

        memcpy(Buffer, &Value, AccessWidth >> 3);
        Buffer += AccessWidth >> 3;
    }

    return 1;
}
