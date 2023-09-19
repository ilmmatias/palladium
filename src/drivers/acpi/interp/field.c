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
static uint64_t ReadRegion(AcpiValue *Source, int Offset, int Size) {
    switch (Source->Region.RegionSpace) {
        /* SystemMemory; We'll be reading a physical memory address. */
        case 0: {
            void *Address = MI_PADDR_TO_VADDR(Source->Region.RegionOffset + Offset);
            switch (Size) {
                case 2:
                    return *(uint16_t *)Address;
                case 4:
                    return *(uint32_t *)Address;
                case 8:
                    return *(uint64_t *)Address;
                default:
                    return *(uint8_t *)Address;
            }
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
static void WriteRegion(AcpiValue *Source, int Offset, int Size, uint64_t Data) {
    switch (Source->Region.RegionSpace) {
        /* SystemMemory; We'll be writing into a physical memory address. */
        case 0: {
            void *Address = MI_PADDR_TO_VADDR(Source->Region.RegionOffset + Offset);
            switch (Size) {
                case 2:
                    *(uint16_t *)Address = Data;
                    break;
                case 4:
                    *(uint32_t *)Address = Data;
                    break;
                case 8:
                    *(uint64_t *)Address = Data;
                    break;
                default:
                    *(uint8_t *)Address = Data;
                    break;
            }
        }

        default:
            printf(
                "WriteRegionField(%p, %u, %u, %llu), RegionSpace = %hhu\n",
                Source,
                Offset,
                Size,
                Data,
                Source->Region.RegionSpace);
            while (1)
                ;
    }
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     Internal function to read field data; Used within the main loop of AcpipRead/WriteField.
 *
 * PARAMETERS:
 *     Source - AML value containing a region field.
 *     Offset - Byte offset within the region.
 *     AccessWidth - How many bits we're trying to read.
 *
 * RETURN VALUE:
 *     Requested data.
 *-----------------------------------------------------------------------------------------------*/
static uint64_t ReadField(AcpiValue *Source, int Offset, int AccessWidth) {
    switch (Source->FieldUnit.FieldType) {
        case ACPI_FIELD:
            return ReadRegion(&Source->FieldUnit.Region->Value, Offset, AccessWidth >> 3);

        case ACPI_INDEX_FIELD: {
            /* Index field means we need to write into the index location, followed by R/W'ing
               from/into the data location. */

            AcpiValue Index;
            Index.Type = ACPI_INTEGER;
            Index.Integer = Offset;
            AcpipWriteField(&Source->FieldUnit.Region->Value, &Index);

            AcpiValue Target;
            AcpipReadField(&Source->FieldUnit.Data->Value, &Target);

            return Target.Integer;
        }
    }

    return 0;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     Internal function to write field data; Used within the main loop of AcpipWriteField.
 *
 * PARAMETERS:
 *     Source - AML value containing a region field.
 *     Offset - Byte offset within the region.
 *     AccessWidth - How many bits we're trying to write.
 *     Data - The value we're trying to write.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
static void WriteField(AcpiValue *Target, int Offset, int AccessWidth, uint64_t Data) {
    switch (Target->FieldUnit.FieldType) {
        case ACPI_FIELD:
            return WriteRegion(&Target->FieldUnit.Region->Value, Offset, AccessWidth >> 3, Data);

        case ACPI_INDEX_FIELD: {
            /* Index field means we need to write into the index location, followed by R/W'ing
               from/into the data location. */

            AcpiValue Index;
            Index.Type = ACPI_INTEGER;
            Index.Integer = Offset;
            AcpipWriteField(&Target->FieldUnit.Region->Value, &Index);

            AcpiValue Value;
            Value.Type = ACPI_INTEGER;
            Value.Integer = Data;
            AcpipWriteField(&Target->FieldUnit.Data->Value, &Value);

            break;
        }
    }
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function reads the data inside a given region field; Do not use this for buffer fields.
 *
 * PARAMETERS:
 *     Source - AML value containing a region field.
 *     Target - Where to save the requested data.
 *
 * RETURN VALUE:
 *     1 on success, 0 otherwise.
 *-----------------------------------------------------------------------------------------------*/
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
       merging them into the buffer as we proceed.
       The buffer might start unaligned, and as such, we might need to read two entries before
       writing into the buffer. We'll just assume we always need to do that. */

    int AlignedItemCount = (Source->FieldUnit.Length + AccessWidth - 1) / AccessWidth;

    int UnalignedOffset = Source->FieldUnit.Offset & (AccessWidth - 1);
    int UnalignedLength = Source->FieldUnit.Length & (AccessWidth - 1);
    int UnalignedItemCount =
        (Source->FieldUnit.Length + UnalignedOffset + AccessWidth - 1) / AccessWidth;

    uint64_t Item =
        ReadField(Source, (Source->FieldUnit.Offset >> 3), AccessWidth) >> UnalignedOffset;

    for (int i = 1; i < UnalignedItemCount; i++) {
        int Offset = (Source->FieldUnit.Offset >> 3) + (AccessWidth >> 3) * i;
        uint64_t Value = ReadField(Source, Offset, AccessWidth);

        /* Unaligned start, merge with previous item. */
        if (UnalignedOffset) {
            Item |= Value << (AccessWidth - UnalignedOffset);
        }

        /* On unaligned buffers, we'll overshoot as we need one extra entry to mount the item;
           Break out if that's the case and we already got the required extra entry. */
        if (i == AlignedItemCount) {
            break;
        }

        memcpy(Buffer, &Item, AccessWidth >> 3);
        Buffer += AccessWidth >> 3;
        Item = Value >> UnalignedOffset;
    }

    /* Mask-off anything beyond our length, in case we have an unaligned size. */
    if (UnalignedLength) {
        Item &= (2ull << (UnalignedLength - 1)) - 1;
    }

    memcpy(Buffer, &Item, AccessWidth >> 3);
    return 1;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function writes the data from the given buffer into the given region field; Do not use
 *     this for buffer fields.
 *
 * PARAMETERS:
 *     Source - AML value containing a region field.
 *     Data - What to write into the field.
 *
 * RETURN VALUE:
 *     1 on success, 0 otherwise.
 *-----------------------------------------------------------------------------------------------*/
int AcpipWriteField(AcpiValue *Target, AcpiValue *Data) {
    (void)WriteField;
    printf("AcpipWriteField(%p, %p)\n", Target, Data);
    while (1)
        ;
}
