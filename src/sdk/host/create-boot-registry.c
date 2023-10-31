/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#include <registry.h>
#include <rt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint8_t BlockBuffer[REG_BLOCK_SIZE];

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function searches for a free entry with proper size inside the current index tree.
 *
 * PARAMETERS:
 *     Stream - File stream where the registry is located at.
 *     FirstBlockOffset - Root of the index tree.
 *     Length - Minimum size of the entry.
 *     FoundBlockOffset - Output; Block offset (SEEK_SET) where the entry is located.
 *
 * RETURN VALUE:
 *     Pointer to first byte of the entry, or NULL on failure.
 *-----------------------------------------------------------------------------------------------*/
static uint8_t *FindFreeEntry(
    FILE *Stream,
    uint32_t FirstBlockOffset,
    uint32_t Length,
    uint32_t *FoundBlockOffset) {
    /* The current version of the registry doesn't support multi-block values. */
    if (Length > REG_BLOCK_SIZE - sizeof(RegBlockHeader)) {
        return NULL;
    }

    RegBlockHeader *BlockHeader = (RegBlockHeader *)BlockBuffer;
    uint32_t CurrentBlockOffset = FirstBlockOffset;
    if (fseek(Stream, CurrentBlockOffset, SEEK_SET) ||
        fread(BlockBuffer, REG_BLOCK_SIZE, 1, Stream) != 1) {
        return NULL;
    }

    /* Linear search starting from the hint index (minimum index where we can find free space).
       On failure to find, we try to allocate a new block at the end of the file (which is
       guaranteed to fit at least the required entry).  */
    int ForceSkip = 0;
    do {
        while (ForceSkip || BlockHeader->InsertOffsetHint == UINT32_MAX) {
            CurrentBlockOffset = BlockHeader->OffsetToNextBlock;

            if (fseek(Stream, CurrentBlockOffset, SEEK_SET) ||
                fread(BlockBuffer, REG_BLOCK_SIZE, 1, Stream) != 1) {
                return NULL;
            }

            ForceSkip = 0;
        }

        uint8_t *Entry = (uint8_t *)(BlockHeader + 1) + BlockHeader->InsertOffsetHint;
        while (Entry - BlockBuffer < REG_BLOCK_SIZE) {
            RegEntryHeader *EntryHeader = (RegEntryHeader *)Entry;

            if (EntryHeader->Type == REG_ENTRY_REMOVED && EntryHeader->Length >= Length) {
                *FoundBlockOffset = CurrentBlockOffset;
                return Entry;
            }

            Entry += EntryHeader->Length;
        }

        ForceSkip = 1;
    } while (BlockHeader->OffsetToNextBlock);

    if (fseek(Stream, 0, SEEK_END)) {
        return NULL;
    }

    BlockHeader->OffsetToNextBlock = ftell(Stream);
    *FoundBlockOffset = BlockHeader->OffsetToNextBlock;

    if (fseek(Stream, CurrentBlockOffset, SEEK_SET) ||
        fwrite(BlockBuffer, REG_BLOCK_SIZE, 1, Stream) != 1) {
        return NULL;
    }

    memset(BlockBuffer, 0, REG_BLOCK_SIZE);
    memcpy(BlockHeader->Signature, REG_BLOCK_SIGNATURE, 4);
    ((RegEntryHeader *)(BlockHeader + 1))->Length = REG_BLOCK_SIZE - sizeof(RegBlockHeader);

    return (uint8_t *)(BlockHeader + 1);
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function writes back the block buffer into the disk, adjusting the current block
 *     length and hint index.
 *
 * PARAMETERS:
 *     Stream - File stream where the registry is located at.
 *     Entry - Pointer to where the entry we wrote starts.
 *     Block - SEEK_SET offset to the start of the block where the entry is located.
 *     Length - Length of the entry.
 *     OldLength - Length of the free entry that used to occupy the slot.
 *
 * RETURN VALUE:
 *     1 on success, 0 otherwise.
 *-----------------------------------------------------------------------------------------------*/
static int ConsolidateEntry(
    FILE *Stream,
    uint8_t *Entry,
    uint32_t Block,
    uint32_t Length,
    uint32_t OldLength) {
    RegBlockHeader *BlockHeader = (RegBlockHeader *)BlockBuffer;
    RegEntryHeader *EntryHeader = (RegEntryHeader *)Entry;

    /* We have three different cases to consider:
        - No more space left in the block; Up until something is removed, the insert hint is
           set to `no more entries`.
        - Not enough space for a new empty entry (capable of containing a small name +
          uint8_t). Merge the free space into the entry.
        - Enough space for a new entry; Use the remaining space for it. */
    if (Entry + Length - BlockBuffer < REG_BLOCK_SIZE) {
        if (OldLength - Length >= sizeof(RegEntryHeader)) {
            EntryHeader = (RegEntryHeader *)(Entry + Length);
            EntryHeader->Type = REG_ENTRY_REMOVED;
            EntryHeader->Length = OldLength - Length;
            BlockHeader->InsertOffsetHint = Entry - BlockBuffer + Length - sizeof(RegBlockHeader);
        } else {
            EntryHeader->Length = OldLength;
            BlockHeader->InsertOffsetHint =
                Entry - BlockBuffer + OldLength - sizeof(RegBlockHeader);
        }
    } else {
        BlockHeader->InsertOffsetHint = UINT32_MAX;
    }

    return !fseek(Stream, Block, SEEK_SET) && fwrite(BlockBuffer, REG_BLOCK_SIZE, 1, Stream) == 1;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function creates a R/W registry file, setting up the file header and an empty index
 *     root.
 *
 * PARAMETERS:
 *     Path - Where to create the file.
 *
 * RETURN VALUE:
 *     File stream on success, NULL otherwise.
 *-----------------------------------------------------------------------------------------------*/
static FILE *CreateRegistry(const char *Path) {
    FILE *Stream = fopen(Path, "w+");
    if (!Stream) {
        return NULL;
    }

    RegFileHeader *FileHeader = (RegFileHeader *)BlockBuffer;
    RegBlockHeader *BlockHeader = (RegBlockHeader *)BlockBuffer;

    memset(BlockBuffer, 0, sizeof(RegFileHeader));
    memcpy(FileHeader->Signature, REG_FILE_SIGNATURE, 4);
    if (fwrite(BlockBuffer, sizeof(RegFileHeader), 1, Stream) != 1) {
        fclose(Stream);
        return NULL;
    }

    memset(BlockBuffer, 0, REG_BLOCK_SIZE);
    memcpy(BlockHeader->Signature, REG_BLOCK_SIGNATURE, 4);
    ((RegEntryHeader *)(BlockHeader + 1))->Length = REG_BLOCK_SIZE - sizeof(RegBlockHeader);
    if (fwrite(BlockBuffer, REG_BLOCK_SIZE, 1, Stream) != 1) {
        fclose(Stream);
        return NULL;
    }

    return Stream;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function creates a new registry entry, with the data set to an integer of the
 *     specified size.
 *
 * PARAMETERS:
 *     Stream - File stream where the registry is located at.
 *     FirstBlockOffset - Root of the index tree.
 *     Name - Name of the registry entry.
 *     Type - REG_ENTRY_BYTE/WORD/DWORD/QWORD according to the size of the integer, or 0 for
 *            autodetect.
 *     Value - Data to be stored on the entry.
 *
 * RETURN VALUE:
 *     1 on success, 0 otherwise.
 *-----------------------------------------------------------------------------------------------*/
static int CreateIntegerKey(
    FILE *Stream,
    uint32_t FirstBlockOffset,
    const char *Name,
    uint8_t Type,
    uint64_t Value) {
    uint32_t NameLength = strlen(Name) + 1;

    if (!Stream || NameLength > REG_NAME_SIZE) {
        return 0;
    } else if (Type < REG_ENTRY_BYTE || Type > REG_ENTRY_QWORD) {
        Type = Value < 0x100         ? REG_ENTRY_BYTE
               : Value < 0x10000     ? REG_ENTRY_WORD
               : Value < 0x100000000 ? REG_ENTRY_DWORD
                                     : REG_ENTRY_QWORD;
    }

    uint32_t Block;
    RegBlockHeader *BlockHeader = (RegBlockHeader *)BlockBuffer;
    uint32_t Length = sizeof(RegEntryHeader) + NameLength + (1 << (Type - 1));

    uint8_t *Entry = FindFreeEntry(Stream, FirstBlockOffset, Length, &Block);
    if (!Entry) {
        return 0;
    }

    RegEntryHeader *EntryHeader = (RegEntryHeader *)Entry;
    uint32_t OldLength = EntryHeader->Length;

    EntryHeader->Type = Type;
    EntryHeader->Length = Length;
    EntryHeader->NameHash = RtGetHash(Name, NameLength - 1);
    memcpy(EntryHeader + 1, Name, NameLength);

    switch (Type) {
        case REG_ENTRY_BYTE:
            *(Entry + sizeof(RegEntryHeader) + NameLength) = Value;
            break;
        case REG_ENTRY_WORD:
            *((uint16_t *)(Entry + sizeof(RegEntryHeader) + NameLength)) = Value;
            break;
        case REG_ENTRY_DWORD:
            *((uint32_t *)(Entry + sizeof(RegEntryHeader) + NameLength)) = Value;
            break;
        default:
            *((uint64_t *)(Entry + sizeof(RegEntryHeader) + NameLength)) = Value;
            break;
    }

    return ConsolidateEntry(Stream, Entry, Block, Length, OldLength);
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function creates a new registry entry, with the data set to a NULL-terminated string.
 *
 * PARAMETERS:
 *     Stream - File stream where the registry is located at.
 *     FirstBlockOffset - Root of the index tree.
 *     Name - Name of the registry entry.
 *     Value - NULL-terminated string to be copied into the entry.
 *
 * RETURN VALUE:
 *     1 on success, 0 otherwise.
 *-----------------------------------------------------------------------------------------------*/
static int
CreateStringKey(FILE *Stream, uint32_t FirstBlockOffset, const char *Name, const char *Value) {
    uint32_t NameLength = strlen(Name) + 1;
    uint32_t ValueLength = strlen(Value) + 1;

    if (!Stream || NameLength > REG_NAME_SIZE) {
        return 0;
    }

    uint32_t Block;
    RegBlockHeader *BlockHeader = (RegBlockHeader *)BlockBuffer;
    uint32_t Length = sizeof(RegEntryHeader) + NameLength + ValueLength;

    uint8_t *Entry = FindFreeEntry(Stream, FirstBlockOffset, Length, &Block);
    if (!Entry) {
        return 0;
    }

    RegEntryHeader *EntryHeader = (RegEntryHeader *)Entry;
    uint32_t OldLength = EntryHeader->Length;

    EntryHeader->Type = REG_ENTRY_STRING;
    EntryHeader->Length = Length;
    EntryHeader->NameHash = RtGetHash(Name, NameLength - 1);
    memcpy(EntryHeader + 1, Name, NameLength);
    memcpy(Entry + sizeof(RegEntryHeader) + NameLength, Value, ValueLength);

    return ConsolidateEntry(Stream, Entry, Block, Length, OldLength);
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function creates a new registry subkey, attaching it to the given parent index.
 *
 * PARAMETERS:
 *     Stream - File stream where the registry is located at.
 *     FirstBlockOffset - Root of the parent index tree.
 *     Name - Name of the new subkey.
 *     FirstSubKeyBlockOffset - Output; Offset to the root of the subkey index tree.
 *
 * RETURN VALUE:
 *     1 on success, 0 otherwise.
 *-----------------------------------------------------------------------------------------------*/
static int CreateSubKey(
    FILE *Stream,
    uint32_t FirstBlockOffset,
    const char *Name,
    uint32_t *FirstSubKeyBlockOffset) {
    uint32_t NameLength = strlen(Name) + 1;

    if (!Stream || !FirstSubKeyBlockOffset || NameLength > REG_NAME_SIZE) {
        return 0;
    }

    uint32_t Block;
    RegBlockHeader *BlockHeader = (RegBlockHeader *)BlockBuffer;
    uint32_t Length = sizeof(RegEntryHeader) + NameLength + 4;

    uint8_t *Entry = FindFreeEntry(Stream, FirstBlockOffset, Length, &Block);
    if (!Entry || fseek(Stream, 0, SEEK_END)) {
        return 0;
    }

    *FirstSubKeyBlockOffset = ftell(Stream);

    RegEntryHeader *EntryHeader = (RegEntryHeader *)Entry;
    uint32_t OldLength = EntryHeader->Length;

    EntryHeader->Type = REG_ENTRY_KEY;
    EntryHeader->Length = Length;
    EntryHeader->NameHash = RtGetHash(Name, NameLength - 1);
    memcpy(EntryHeader + 1, Name, NameLength);
    *((uint32_t *)(Entry + sizeof(RegEntryHeader) + NameLength)) = *FirstSubKeyBlockOffset;
    if (!ConsolidateEntry(Stream, Entry, Block, Length, OldLength)) {
        return 0;
    }

    memset(BlockBuffer, 0, REG_BLOCK_SIZE);
    memcpy(BlockHeader->Signature, REG_BLOCK_SIGNATURE, 4);
    ((RegEntryHeader *)(BlockHeader + 1))->Length = REG_BLOCK_SIZE - sizeof(RegBlockHeader);

    return !fseek(Stream, *FirstSubKeyBlockOffset, SEEK_SET) &&
           fwrite(BlockBuffer, REG_BLOCK_SIZE, 1, Stream) == 1;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function creates a standard boot registry file, saving it to $pwd/_root/bootmgr.reg.
 *
 * PARAMETERS:
 *     None.
 *
 * RETURN VALUE:
 *     Non-zero on failure, zero otherwise.
 *-----------------------------------------------------------------------------------------------*/
int CreateBootRegistry(void) {
    FILE *Stream = CreateRegistry("_root/bootmgr.reg");
    if (!Stream) {
        return 1;
    }

    uint32_t Root = sizeof(RegFileHeader);
    if (!CreateIntegerKey(Stream, Root, "Timeout", REG_ENTRY_DWORD, 5) ||
        !CreateIntegerKey(Stream, Root, "DefaultSelection", REG_ENTRY_DWORD, 0)) {
        fclose(Stream);
        return 1;
    }

    uint32_t Entries;
    if (!CreateSubKey(Stream, Root, "Entries", &Entries)) {
        fclose(Stream);
        return 1;
    }

    uint32_t Entry;
    if (!CreateSubKey(Stream, Entries, "Boot from the Installation Disk", &Entry) ||
        !CreateIntegerKey(Stream, Entry, "Type", REG_ENTRY_DWORD, 0) ||
        !CreateStringKey(Stream, Entry, "SystemFolder", "boot()/System")) {
        fclose(Stream);
        return 1;
    }

    if (!CreateSubKey(Stream, Entries, "Boot from the First Hard Disk", &Entry) ||
        !CreateIntegerKey(Stream, Entry, "Type", REG_ENTRY_DWORD, 1) ||
        !CreateStringKey(Stream, Entry, "BootDevice", "bios(80)")) {
        fclose(Stream);
        return 1;
    }

    fclose(Stream);
    return 0;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function creates a kernel registry file, tuned for all configs supported by
 *     test-run.sh.
 *
 * PARAMETERS:
 *     None.
 *
 * RETURN VALUE:
 *     Non-zero on failure, zero otherwise.
 *-----------------------------------------------------------------------------------------------*/
int CreateKernelRegistry(void) {
    FILE *Stream = CreateRegistry("_root/System/kernel.reg");
    if (!Stream) {
        return 1;
    }

    uint32_t Root = sizeof(RegFileHeader);
    uint32_t Entries;
    if (!CreateSubKey(Stream, Root, "Drivers", &Entries)) {
        fclose(Stream);
        return 1;
    }

    if (!CreateIntegerKey(Stream, Entries, "acpi.sys", REG_ENTRY_DWORD, 1) ||
        !CreateIntegerKey(Stream, Entries, "pci.sys", REG_ENTRY_DWORD, 1)) {
        fclose(Stream);
        return 1;
    }

    fclose(Stream);
    return 0;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function creates a standard boot registry file, saving it to $pwd/_root/bootmgr.reg.
 *
 * PARAMETERS:
 *     None.
 *
 * RETURN VALUE:
 *     Non-zero on failure, zero otherwise.
 *-----------------------------------------------------------------------------------------------*/
int main(void) {
    CreateBootRegistry();
    CreateKernelRegistry();
}
