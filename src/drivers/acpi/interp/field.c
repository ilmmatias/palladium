/* SPDX-FileCopyrightText: (C) 2023-2026 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <acpi.h>
#include <acpip.h>
#include <stdint.h>
#include <string.h>

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
 *     true/false depending on success.
 *-----------------------------------------------------------------------------------------------*/
static bool SetupPciConfigRegion(AcpiObject *Object) {
    if (Object->Value.Region.RegionSpace != 2 || Object->Value.Region.PciReady) {
        return true;
    }

    /* _SEG and _BBN makes no sense to exist inside a sub device, they should be taken from
       the root bus instead. */
    AcpiObject *RootBus = Object;
    while (RootBus) {
        AcpiValue Id;

        if (AcpiEvaluateObject(AcpiSearchObject(RootBus, "_HID"), &Id, ACPI_INTEGER) ||
            AcpiEvaluateObject(AcpiSearchObject(RootBus, "_CID"), &Id, ACPI_INTEGER)) {
            if (Id.Integer == 0x030AD041 || Id.Integer == 0x080AD041) {
                break;
            }
        }

        RootBus = RootBus->Parent;
    }

    /* It makes no sense for the root PCI bus to not exist. */
    if (!RootBus) {
        return false;
    }

    /* _ADR is the only required symbol, as it the device and function values (which we can't
       obtain any other way, and can't assume to be zero).
       _SEG and _BBN will be also used if they're found, but they'll be taken as 0 (root bus)
       otherwise. */

    AcpiValue Adr;
    AcpiName Name;
    Name.LinkedObject = Object;
    Name.Start = (const uint8_t *)"_ADR";
    Name.BacktrackCount = 0;
    Name.SegmentCount = 1;
    if (!AcpiEvaluateObject(AcpipResolveObject(&Name), &Adr, ACPI_INTEGER)) {
        return false;
    }

    AcpiValue Seg;
    if (AcpiEvaluateObject(AcpiSearchObject(RootBus, "_SEG"), &Seg, ACPI_INTEGER)) {
        Object->Value.Region.PciSegment = Seg.Integer;
    }

    AcpiValue Bbn;
    if (AcpiEvaluateObject(AcpiSearchObject(RootBus, "_BBN"), &Bbn, ACPI_INTEGER)) {
        Object->Value.Region.PciBus = Bbn.Integer;
    }

    /* Everything seems to be valid, break down the _ADR value, and set this region as ready. */
    Object->Value.Region.PciDevice = (Adr.Integer >> 16) & UINT16_MAX;
    Object->Value.Region.PciFunction = Adr.Integer & UINT16_MAX;
    Object->Value.Region.PciReady = true;
    return true;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function sets up a region for reading/writing into the Embedded Controller space.
 *     We're a noop if the region isn't EC, or if we have already cached the required values.
 *
 * PARAMETERS:
 *     Object - AML object containing the region.
 *
 * RETURN VALUE:
 *     true/false depending on success.
 *-----------------------------------------------------------------------------------------------*/
static bool SetupEcRegion(AcpiObject *Object) {
    if (Object->Value.Region.RegionSpace != ACPI_SPACE_EMBEDDED_CONTROL ||
        Object->Value.Region.EcReady) {
        return true;
    }

    /* Find the EC device (parent of the region) by checking for _HID = PNP0C09. */
    AcpiObject *EcDevice = Object->Parent;
    while (EcDevice) {
        AcpiValue Hid;
        if (AcpiEvaluateObject(AcpiSearchObject(EcDevice, "_HID"), &Hid, ACPI_INTEGER)) {
            if (Hid.Integer == 0x090CD041) {
                break;
            }
        }

        EcDevice = EcDevice->Parent;
    }

    /* Use the default values if we couldn't find the device. */
    if (!EcDevice) {
        Object->Value.Region.EcDataPort = 0x62;
        Object->Value.Region.EcCmdPort = 0x66;
        Object->Value.Region.EcReady = true;
        return true;
    }

    /* If we did find it, we need to evaluate the _CRS binary resource. */
    AcpiValue Crs;
    if (!AcpiEvaluateObject(AcpiSearchObject(EcDevice, "_CRS"), &Crs, ACPI_BUFFER)) {
        Object->Value.Region.EcDataPort = 0x62;
        Object->Value.Region.EcCmdPort = 0x66;
        Object->Value.Region.EcReady = true;
        return true;
    }

    /* And parse it for the IO ports... */
    uint8_t *Data = Crs.Buffer->Data;
    uint64_t Size = Crs.Buffer->Size;
    uint16_t Ports[2] = {0x62, 0x66};
    int PortIndex = 0;

    while (Size > 0 && PortIndex < 2) {
        uint8_t Tag = *Data;

        if (Tag == 0x79) {
            /* End tag. */
            break;
        }

        if ((Tag & 0x80) == 0) {
            /* Small resource descriptor. */
            uint64_t Length = Tag & 0x07;
            int Type = (Tag >> 3) & 0x0F;

            if (Size < Length + 1) {
                break;
            }

            if (PortIndex < 2) {
                if (Type == 0x08 && Length >= 7) {
                    Ports[PortIndex++] = *(uint16_t *)(Data + 2);
                } else if (Type == 0x09 && Length >= 3) {
                    Ports[PortIndex++] = *(uint16_t *)(Data + 1);
                }
            }

            Data += Length + 1;
            Size -= Length + 1;
        } else {
            /* Large resource descriptor. */
            if (Size < 3) {
                break;
            }

            uint64_t Length = *(uint16_t *)(Data + 1);
            if (Size < Length + 3) {
                break;
            }

            Data += Length + 3;
            Size -= Length + 3;
        }
    }

    AcpiRemoveReference(&Crs, false);
    Object->Value.Region.EcDataPort = Ports[0];
    Object->Value.Region.EcCmdPort = Ports[1];
    Object->Value.Region.EcReady = true;
    return true;
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
        case ACPI_SPACE_SYSTEM_MEMORY: {
            return AcpipReadMmioSpace(Source->Region.RegionOffset + Offset, Size);
        }

        case ACPI_SPACE_SYSTEM_IO: {
            return AcpipReadIoSpace(Source->Region.RegionOffset + Offset, Size);
        }

        case ACPI_SPACE_PCI_CONFIG: {
            return AcpipReadPciConfigSpace(Source, Source->Region.RegionOffset + Offset, Size);
        }

        case ACPI_SPACE_EMBEDDED_CONTROL: {
            uint64_t Value = 0;
            for (int i = 0; i < Size; i++) {
                Value |= AcpipReadEcSpace(
                             Source->Region.EcCmdPort,
                             Source->Region.EcDataPort,
                             Source->Region.RegionOffset + Offset + i)
                         << (i * 8);
            }

            return Value;
        }

        case ACPI_SPACE_SYSTEM_CMOS: {
            uint64_t Value = 0;
            for (int i = 0; i < Size; i++) {
                Value |= AcpipReadCmosSpace(Source->Region.RegionOffset + Offset + i) << (i * 8);
            }

            return Value;
        }

        default: {
            AcpipShowErrorMessage(
                ACPI_REASON_CORRUPTED_TABLES,
                "ReadRegionField(%p, %u, %u), RegionSpace = %hhu\n",
                Source,
                Offset,
                Size,
                Source->Region.RegionSpace);
        }
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
        case ACPI_SPACE_SYSTEM_MEMORY: {
            AcpipWriteMmioSpace(Source->Region.RegionOffset + Offset, Size, Data);
            break;
        }

        case ACPI_SPACE_SYSTEM_IO: {
            AcpipWriteIoSpace(Source->Region.RegionOffset + Offset, Size, Data);
            break;
        }

        case ACPI_SPACE_PCI_CONFIG: {
            AcpipWritePciConfigSpace(Source, Source->Region.RegionOffset + Offset, Size, Data);
            break;
        }

        case ACPI_SPACE_EMBEDDED_CONTROL: {
            for (int i = 0; i < Size; i++) {
                AcpipWriteEcSpace(
                    Source->Region.EcCmdPort,
                    Source->Region.EcDataPort,
                    Source->Region.RegionOffset + Offset + i,
                    (Data >> (i * 8)) & UINT8_MAX);
            }

            break;
        }

        case ACPI_SPACE_SYSTEM_CMOS: {
            for (int i = 0; i < Size; i++) {
                AcpipWriteCmosSpace(
                    Source->Region.RegionOffset + Offset + i, (Data >> (i * 8)) & UINT8_MAX);
            }

            break;
        }

        default: {
            AcpipShowErrorMessage(
                ACPI_REASON_CORRUPTED_TABLES,
                "WriteRegionField(%p, %u, %u, %lu), RegionSpace = %hhu\n",
                Source,
                Offset,
                Size,
                Data,
                Source->Region.RegionSpace);
        }
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
        case ACPI_FIELD: {
            return ReadRegion(&Source->FieldUnit.Region->Value, Offset, AccessWidth / 8);
        }

        case ACPI_BANK_FIELD: {
            /* BankField: Write BankValue to bank register, then R/W from/into the region. */
            AcpiValue BankValueData;
            BankValueData.Type = ACPI_INTEGER;
            BankValueData.References = 1;
            BankValueData.Integer = Source->FieldUnit.BankValue;
            AcpipWriteField(&Source->FieldUnit.Data->Value, &BankValueData);
            return ReadRegion(&Source->FieldUnit.Region->Value, Offset, AccessWidth / 8);
        }

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
        /* Preserve
         * For this one, do WriteAsZeroes if we have no mask, as reading from certain types of
         * regions (when that's not expected) might cause issues. */
        case 0:
            Base = Mask ? ReadField(Target, Offset, AccessWidth) : 0;
            break;

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
        case ACPI_FIELD: {
            return WriteRegion(
                &Target->FieldUnit.Region->Value, Offset, AccessWidth / 8, MaskedValue);
        }

        case ACPI_BANK_FIELD: {
            /* BankField: Write BankValue to bank register, then R/W to/from the region. */
            AcpiValue BankValueData;
            BankValueData.Type = ACPI_INTEGER;
            BankValueData.References = 1;
            BankValueData.Integer = Target->FieldUnit.BankValue;
            AcpipWriteField(&Target->FieldUnit.Data->Value, &BankValueData);
            return WriteRegion(
                &Target->FieldUnit.Region->Value, Offset, AccessWidth / 8, MaskedValue);
        }

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
    int ByteCount = (RemainingBits > AccessWidth ? AccessWidth : RemainingBits) / 8;
    if (ByteCount > 0) {
        memcpy(&Value, Buffer, ByteCount);
    }

    return Value;
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
 *     true/false depending on success.
 *-----------------------------------------------------------------------------------------------*/
bool AcpipReadField(AcpiValue *Source, AcpiValue *Target) {
    /* TODO: I think this deserves a macro or at least some struct/union magic? */
    bool NeedLock = (Source->FieldUnit.AccessType >> 4) & 1;
    if (NeedLock) {
        AcpipAcquireGlobalLock();
    }

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

    /* We need some extra work for PCI Config and EC regions, but we'll be caching it afterwards. */
    if (Source->FieldUnit.FieldType == ACPI_FIELD ||
        Source->FieldUnit.FieldType == ACPI_BANK_FIELD) {
        if (!SetupPciConfigRegion(Source->FieldUnit.Region) ||
            !SetupEcRegion(Source->FieldUnit.Region)) {
            if (NeedLock) {
                AcpipReleaseGlobalLock();
            }

            return false;
        }
    }

    /* Anything over 64-bits doesn't fit inside an integer; Otherwise, we don't need to allocate
       any extra memory. */
    uint8_t *Buffer;
    if (Source->FieldUnit.Length > 64) {
        int BufferSize = (Source->FieldUnit.Length + AccessWidth - 1) / 8;

        Target->Type = ACPI_BUFFER;
        Target->Buffer = AcpipAllocateBlock(sizeof(AcpiBuffer) + BufferSize);
        if (!Target->Buffer) {
            return false;
        }

        Target->Buffer->References = 1;
        Target->Buffer->Size = BufferSize;
        memset(Target->Buffer->Data, 0, BufferSize);

        Buffer = Target->Buffer->Data;
    } else {
        Target->Type = ACPI_INTEGER;
        Target->References = 1;
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

    uint64_t Item = ReadField(Source, Source->FieldUnit.Offset / 8, AccessWidth) >> UnalignedOffset;

    for (int i = 1; i < UnalignedItemCount; i++) {
        int Offset = (Source->FieldUnit.Offset / 8) + (AccessWidth / 8) * i;
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

        memcpy(Buffer, &Item, AccessWidth / 8);
        Buffer += AccessWidth / 8;
        Item = Value >> UnalignedOffset;
    }

    /* Mask-off anything beyond our length, in case we have an unaligned size. */
    if (UnalignedLength) {
        Item &= (2ULL << (UnalignedLength - 1)) - 1;
    }

    memcpy(Buffer, &Item, AccessWidth / 8);

    if (NeedLock) {
        AcpipReleaseGlobalLock();
    }

    return true;
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
 *     true/false depending on success.
 *-----------------------------------------------------------------------------------------------*/
bool AcpipWriteField(AcpiValue *Target, AcpiValue *Data) {
    bool NeedLock = (Target->FieldUnit.AccessType >> 4) & 1;
    if (NeedLock) {
        AcpipAcquireGlobalLock();
    }

    /* We need to check for access width > 1 byte; Any invalid value will also be assumed to be 1
       byte. */
    uint32_t AccessWidth = 8;
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

    /* We need some extra work for PCI Config and EC regions, but we'll be caching it afterwards. */
    if (Target->FieldUnit.FieldType == ACPI_FIELD ||
        Target->FieldUnit.FieldType == ACPI_BANK_FIELD) {
        if (!SetupPciConfigRegion(Target->FieldUnit.Region) ||
            !SetupEcRegion(Target->FieldUnit.Region)) {
            if (NeedLock) {
                AcpipReleaseGlobalLock();
            }

            return false;
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
        Buffer = (const uint8_t *)&Data->String->Data;
        BufferWidth = strlen(Data->String->Data) * 8 + 8;
    } else {
        Buffer = Data->Buffer->Data;
        BufferWidth = Data->Buffer->Size * 8;
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
    int FieldOffset = Target->FieldUnit.Offset / 8;
    int BufferOffset = AccessWidth / 8;

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

        /* On the unaligned loop, we just need to mask off any bits higher than the remaining
           buffer size. */
        int RunOffLength = BufferWidth - BufferOffset * 8;
        if ((int)AccessWidth < RunOffLength) {
            RunOffLength = AccessWidth;
        } else if (RunOffLength < 0) {
            RunOffLength = 0;
        }

        WriteField(Target, FieldOffset, AccessWidth, Item, UINT64_MAX << RunOffLength);
        Buffer += AccessWidth / 8;
        FieldOffset += AccessWidth / 8;
        BufferOffset += AccessWidth / 8;
        Item = Value >> UnalignedOffset;
    }

    /* Fixup the buffer offset, as it points to the item after the aligned end. */
    BufferOffset -= AccessWidth / 8;

    /* This should take care of the remaining buffer size being bigger than the AccessWidth. */
    int RunOffLength = BufferWidth - BufferOffset * 8;
    if (Target->FieldUnit.Length < AccessWidth) {
        if ((int)Target->FieldUnit.Length < RunOffLength) {
            RunOffLength = Target->FieldUnit.Length;
        }
    } else if ((int)(AccessWidth - UnalignedLength) < RunOffLength) {
        RunOffLength = AccessWidth - UnalignedLength;
    } else if (RunOffLength < 0) {
        RunOffLength = 0;
    }

    WriteField(Target, FieldOffset, AccessWidth, Item, UINT64_MAX << RunOffLength);

    if (NeedLock) {
        AcpipReleaseGlobalLock();
    }

    return true;
}
