/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#include <acpip.h>
#include <mm.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function resolves an object, either casting it directly into an integer, or executing
 *     it as a method (and trying casting its return value into an integer), depending on the
 *     type of the object.
 *     We use this to resolve all the required objects for PCI config related setup.
 *
 * PARAMETERS:
 *     Object - Base object; PCI config region.
 *     Name - Name of the object to be resolved.
 *     Result - Output; Will be set to whatever integer we read on success.
 *
 * RETURN VALUE:
 *     1 on success, 0 otherwise.
 *-----------------------------------------------------------------------------------------------*/
static int ReadInteger(AcpiObject *Object, const char *Name, uint64_t *Result) {
    AcpipName RawName;
    RawName.LinkedObject = Object;
    RawName.Start = (const uint8_t *)Name;
    RawName.BacktrackCount = 0;
    RawName.SegmentCount = 1;

    Object = AcpipResolveObject(&RawName);
    if (!Object) {
        return 0;
    }

    if (Object->Value.Type != ACPI_METHOD) {
        return AcpipCastToInteger(&Object->Value, Result, 0);
    }

    AcpiValue Value;
    if (!AcpiExecuteMethod(Object, 0, NULL, &Value)) {
        return 0;
    }

    return AcpipCastToInteger(&Value, Result, 1);
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function sets up a region for reading/writing into the PCI configuration space.
 *     We're a noop if the region isn't PCI_Config, or if we have already cached the required
 *     values.
 *
 * PARAMETERS:
 *     Object - AML object containing the region.
 *
 * RETURN VALUE:
 *     1 on success, 0 otherwise.
 *-----------------------------------------------------------------------------------------------*/
static int SetupPciConfigRegion(AcpiObject *Object) {
    if (Object->Value.Region.RegionSpace != 2 || Object->Value.Region.PciReady) {
        return 1;
    }

    /* _ADR is the only required symbol, as it the device and function values (which we can't
       obtain any other way, and can't assume to be zero).
       _SEG and _BBN will be also used if they're found, but they'll be taken as 0 (root bus)
       otherwise. */

    uint64_t AdrValue;
    if (!ReadInteger(Object, "_ADR", &AdrValue)) {
        return 0;
    }

    uint64_t SegValue;
    if (ReadInteger(Object, "_SEG", &SegValue)) {
        Object->Value.Region.PciSegment = SegValue;
    }

    uint64_t BbnValue;
    if (ReadInteger(Object, "_BBN", &BbnValue)) {
        Object->Value.Region.PciBus = BbnValue;
    }

    /* Everything seems to be valid, break down the _ADR value, and set this region as ready. */
    Object->Value.Region.PciDevice = (AdrValue >> 16) & UINT16_MAX;
    Object->Value.Region.PciFunction = AdrValue & UINT16_MAX;
    Object->Value.Region.PciReady = 1;
    return 1;
}

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

        /* PCI_Config; We need to read from the PCI configuration space; This is OS/architecture
           specific, so redirect to the correct handler. */
        case 2:
            printf("Read from the PCI configuration space\n");
            printf("    PciReady    = %u\n", Source->Region.PciReady);
            printf("    PciDevice   = %u\n", Source->Region.PciDevice);
            printf("    PciFunction = %u\n", Source->Region.PciFunction);
            printf("    PciSegment  = %u\n", Source->Region.PciSegment);
            printf("    PciBus      = %u\n", Source->Region.PciBus);
            while (1)
                ;

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

        /* PCI_Config; We need to write into the PCI configuration space; This is OS/architecture
           specific, so redirect to the correct handler. */
        case 2:
            printf("Write into the PCI configuration space\n");
            printf("    PciReady    = %u\n", Source->Region.PciReady);
            printf("    PciDevice   = %u\n", Source->Region.PciDevice);
            printf("    PciFunction = %u\n", Source->Region.PciFunction);
            printf("    PciSegment  = %u\n", Source->Region.PciSegment);
            printf("    PciBus      = %u\n", Source->Region.PciBus);
            while (1)
                ;

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
 *     RegionOffset - Byte offset within the region.
 *     AccessWidth - How many bits we're trying to write.
 *     Data - The value we're trying to write.
 *     Mask - Base mask used when setting up the actual value to be written into the region.
 *            A `MaskedBase` value will be calculated as `Base & Mask`, while the value to be
 *            written into the region will be calculated as `MaskedBase | Data`.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
static void
WriteField(AcpiValue *Target, int Offset, int AccessWidth, uint64_t Data, uint64_t Mask) {
    uint64_t Base;
    switch ((Target->FieldUnit.AccessType >> 5) & 0x0F) {
        /* Preserve */
        case 0: {
            Base = ReadField(Target, Offset, AccessWidth);
            break;
        }

        /* WriteAsOnes */
        case 1:
            Base = UINT64_MAX;
            break;

        /* WriteAsZeros */
        default:
            Base = 0;
            break;
    }

    uint64_t MaskedBase = Base & Mask;
    uint64_t MaskedValue = MaskedBase | Data;

    switch (Target->FieldUnit.FieldType) {
        case ACPI_FIELD:
            return WriteRegion(
                &Target->FieldUnit.Region->Value, Offset, AccessWidth >> 3, MaskedValue);

        case ACPI_INDEX_FIELD: {
            /* Index field means we need to write into the index location, followed by R/W'ing
               from/into the data location. */

            AcpiValue Index;
            Index.Type = ACPI_INTEGER;
            Index.Integer = Offset;
            AcpipWriteField(&Target->FieldUnit.Region->Value, &Index);

            AcpiValue Value;
            Value.Type = ACPI_INTEGER;
            Value.Integer = MaskedValue;
            AcpipWriteField(&Target->FieldUnit.Data->Value, &Value);

            break;
        }
    }
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     Internal function to safely read data from a buffer; Used within the main loop of
 *     AcpipWriteField, as it might try to read ahead of the buffer end.
 *
 * PARAMETERS:
 *     Buffer - Start of the buffer were trying to read.
 *     Offset - Byte offset within the buffer.
 *     BufferWidth - Size (in bits) of the buffer.
 *     AccessWidth - How many bits we're trying to read.
 *
 * RETURN VALUE:
 *     Data from the buffer.
 *-----------------------------------------------------------------------------------------------*/
static uint64_t
SafeBufferRead(const uint8_t *Buffer, uint64_t Offset, int BufferWidth, int AccessWidth) {
    int RemainingBits = BufferWidth - Offset * 8;
    Buffer += Offset;

    /* This should be guaranteed to be a multiple of 8 (and limited between 0-64). */
    uint64_t Value = 0;
    switch (RemainingBits > AccessWidth ? AccessWidth : RemainingBits) {
        /* Somewhat fixed values (they have no complex logic). */
        case 8:
            return *Buffer;
        case 64:
            return *(uint64_t *)Buffer;

        /* 16-24, read up any bits after the 16th, and join them together. */
        case 24:
            Value |= (uint32_t)(*(Buffer + 2)) << 16;
        case 16:
            return *(uint16_t *)Buffer | Value;

        /* 32-56, read up any bits after the 32nd, and join them together. */
        case 56:
            Value |= (uint64_t)(*(Buffer + 6)) << 48;
        case 48:
            Value |= (uint64_t)(*(Buffer + 5)) << 40;
        case 40:
            Value |= (uint64_t)(*(Buffer + 4)) << 32;
        case 32:
            return *(uint32_t *)Buffer | Value;

        /* Assume anything else (including 0), is just zero. */
        default:
            return 0;
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

    /* We need some extra work for PCI Config regions, but we'll be caching it afterwards. */
    if (Source->FieldUnit.FieldType == ACPI_FIELD) {
        if (!SetupPciConfigRegion(Source->FieldUnit.Region)) {
            return 0;
        }
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
    /* We need to check for access width > 1 byte; Any invalid value will also be assumed to be 1
       byte. */
    int AccessWidth = 8;
    switch (Target->FieldUnit.AccessType & 0x0F) {
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

    /* We need some extra work for PCI Config regions, but we'll be caching it afterwards. */
    if (Target->FieldUnit.FieldType == ACPI_FIELD) {
        if (!SetupPciConfigRegion(Target->FieldUnit.Region)) {
            return 0;
        }
    }

    /* All accepted data/inputs should already be in a "buffer-like" type, so no need for
       conversions. */

    const uint8_t *Buffer;
    int BufferWidth;

    if (Data->Type == ACPI_INTEGER) {
        Buffer = (const uint8_t *)&Data->Integer;
        BufferWidth = 64;
    } else if (Data->Type == ACPI_STRING) {
        Buffer = (const uint8_t *)&Data->String;
        BufferWidth = (strlen(Data->String) << 3) + 8;
    } else {
        Buffer = Data->Buffer.Data;
        BufferWidth = Data->Buffer.Size << 3;
    }

    /* We need to respect the access width, and as such, write item by item into the region,
       reading (and merging) the input into a temporary buffer as we proceed.
       The buffer might start unaligned, and as such, we might need to read two entries before
       writing into the region. We'll just assume we always need to do that. */

    int AlignedItemCount = (Target->FieldUnit.Length + AccessWidth - 1) / AccessWidth;

    int UnalignedOffset = Target->FieldUnit.Offset & (AccessWidth - 1);
    int UnalignedLength = Target->FieldUnit.Length & (AccessWidth - 1);
    int UnalignedItemCount =
        (Target->FieldUnit.Length + UnalignedOffset + AccessWidth - 1) / AccessWidth;

    /* Always take caution with the buffer width when reading anything; Running over it is UB,
       and would probably access memory not belonging to us (or even unmapped).
       SafeBufferRead() should handle that for us. */
    uint64_t Item = SafeBufferRead(Buffer, 0, BufferWidth, AccessWidth) >> UnalignedOffset;
    int FieldOffset = Target->FieldUnit.Offset >> 3;
    int BufferOffset = AccessWidth >> 3;

    for (int i = 1; i < UnalignedItemCount; i++) {
        uint64_t Value = SafeBufferRead(Buffer, BufferOffset, BufferWidth, AccessWidth);

        /* Unaligned start, merge with previous item. */
        if (UnalignedOffset) {
            Item |= Value << (AccessWidth - UnalignedOffset);
        }

        /* On unaligned buffers, we'll overshoot as we need one extra entry to mount the item;
           Break out if that's the case and we already got the required extra entry. */
        if (i == AlignedItemCount) {
            break;
        }

        int RemainingBufferWidth = BufferWidth - (BufferOffset << 3);
        if (RemainingBufferWidth < 0) {
            RemainingBufferWidth = 0;
        }

        /* On the unaligned loop, we just need to mask off any bits higher than the remaining
           buffer size. */
        uint64_t Mask = UINT64_MAX;
        Mask >>= AccessWidth - RemainingBufferWidth;

        WriteField(Target, FieldOffset, AccessWidth, Item, ~Mask);
        Buffer += AccessWidth >> 3;
        FieldOffset += AccessWidth >> 3;
        BufferOffset += AccessWidth >> 3;
        Item = Value >> UnalignedOffset;
    }

    int RemainingBufferWidth = BufferWidth - (BufferOffset << 3);

    if (UnalignedLength < RemainingBufferWidth) {
        RemainingBufferWidth = UnalignedLength;
    }

    if (RemainingBufferWidth < 0) {
        RemainingBufferWidth = 0;
    }

    /* Outside the loop, mask off anything that either goes off the buffer, or off the remaining
       field width. */
    uint64_t Mask = UINT64_MAX;
    Mask >>= AccessWidth - RemainingBufferWidth;

    WriteField(Target, FieldOffset, AccessWidth, Item, Mask);
    return 1;
}
