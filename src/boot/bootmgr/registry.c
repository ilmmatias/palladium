/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#include <boot.h>
#include <memory.h>
#include <registry.h>
#include <rt.h>
#include <string.h>

RegHandle *BmBootRegistry = NULL;

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function sets up the boot manager registry file, used by the menu.
 *
 * PARAMETERS:
 *     None.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void BiInitRegistry(void) {
    BmBootRegistry = BmLoadRegistry("boot()/bootmgr.reg");
    if (!BmBootRegistry) {
        BmPanic("An error occoured while trying to setup the boot manager environment.\n"
                "Could not open the Boot Manager Registry file.\n");
    }
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function loads and validates the registry file at `Path`, returning a handle with its
 *     data.
 *
 * PARAMETERS:
 *     Path - Full path to the registry file.
 *
 * RETURN VALUE:
 *     Pointer to the data handle, or NULL if we failed to load it.
 *-----------------------------------------------------------------------------------------------*/
RegHandle *BmLoadRegistry(const char *Path) {
    if (!Path) {
        return NULL;
    }

    FileContext *Stream = BmOpenFile(Path);
    if (!Stream) {
        return NULL;
    }

    RegHandle *Handle = BmAllocateBlock(sizeof(RegHandle));
    if (!Handle) {
        BmCloseFile(Stream);
        return NULL;
    } else if (BmReadFile(Stream, Handle->Buffer, 0, REG_BLOCK_SIZE, NULL)) {
        BmFreeBlock(Handle);
        BmCloseFile(Stream);
        return NULL;
    }

    RegFileHeader *FileHeader = (RegFileHeader *)Handle->Buffer;
    if (memcmp(FileHeader->Signature, REG_FILE_SIGNATURE, 4)) {
        BmFreeBlock(Handle);
        BmCloseFile(Stream);
        return NULL;
    }

    Handle->Stream = Stream;
    return Handle;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function searches for a specific entry inside the specified key block, validating all
 *     blocks along the way.
 *
 * PARAMETERS:
 *     Handle - Registry handle, as returned by BmLoadRegistry().
 *     KeyBlock - Root of the key's block chain.
 *     Name - Name of the entry to be found.
 *
 * RETURN VALUE:
 *     Pointer to the start of the entry header, or NULL if we failed to load it.
 *-----------------------------------------------------------------------------------------------*/
static RegEntryHeader *FindEntry(RegHandle *Handle, uint32_t KeyBlock, const char *Name) {
    /* All searches are done with a quick hash match (to reduce the search space), followed by a
       a full name comparison (for hash collisions). */
    uint32_t NameHash = RtGetHash(Name, strlen(Name));

    RegBlockHeader *Header = (RegBlockHeader *)Handle->Buffer;
    RegEntryHeader *Entry = NULL;

    while (1) {
        if (!Entry || (char *)Entry - Handle->Buffer >= REG_BLOCK_SIZE) {
            if (Entry) {
                KeyBlock = Header->OffsetToNextBlock;
            }

            if (!KeyBlock ||
                BmReadFile(Handle->Stream, Handle->Buffer, KeyBlock, REG_BLOCK_SIZE, NULL) ||
                memcmp(Header->Signature, REG_BLOCK_SIGNATURE, 4)) {
                return NULL;
            }

            Entry = (RegEntryHeader *)(Header + 1);
        }

        if (Entry->Type != REG_ENTRY_REMOVED && Entry->NameHash == NameHash &&
            !strcmp((const char *)(Entry + 1), Name)) {
            return Entry;
        }

        Entry = (RegEntryHeader *)((char *)Entry + Entry->Length);
    }
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function traverses the loaded registry handle in search of a specific key/value.
 *
 * PARAMETERS:
 *     Handle - Registry handle, as returned by BmLoadRegistry().
 *     Parent - Pointer to the parent entry, as returned by BmGetRegistryEntry() or by ourselves.
 *              Pass NULL to search from the root directory onwards.
 *     Path - Full path to the key or value.
 *
 * RETURN VALUE:
 *     Pointer to a copy of the entry, or NULL if we failed to find it.
 *     The user is expected to manage and free it at the end of its lifetime.
 *-----------------------------------------------------------------------------------------------*/
RegEntryHeader *BmFindRegistryEntry(RegHandle *Handle, RegEntryHeader *Parent, const char *Path) {
    char *CopyPath = BmAllocateBlock(strlen(Path) + 1);
    if (!CopyPath) {
        return NULL;
    }

    strcpy(CopyPath, Path);

    RegEntryHeader *Current = Parent;
    for (char *Segment = strtok(CopyPath, "/"); Segment; Segment = strtok(NULL, "/")) {
        if (Current) {
            if (Current->Type != REG_ENTRY_KEY) {
                return NULL;
            }

            Current =
                FindEntry(Handle, *(uint32_t *)((uint8_t *)Current + Current->Length - 4), Segment);
        } else {
            Current = FindEntry(Handle, sizeof(RegFileHeader), Segment);
        }

        if (!Current) {
            return NULL;
        }
    }

    /* The buffer where the entry is currently located might be reused for loading other entries,
       so we'll be copying it to a new buffer (managed by the caller). */
    RegEntryHeader *Entry = BmAllocateBlock(Current ? Current->Length : sizeof(RegEntryHeader) + 5);

    if (!Entry) {
        return NULL;
    } else if (Current) {
        memcpy(Entry, Current, Current->Length);
    } else {
        Entry->Type = REG_ENTRY_KEY;
        Entry->Length = sizeof(RegEntryHeader) + 5;
        Entry->NameHash = 0;
        *((char *)(Entry + 1)) = 0;
        *((uint32_t *)((uint8_t *)Entry + sizeof(RegEntryHeader) + 1)) = sizeof(RegFileHeader);
    }

    return Entry;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function traverses the loaded registry handle in search of the nth entry of a
 *     specified parent.
 *
 * PARAMETERS:
 *     Handle - Registry handle, as returned by BmLoadRegistry().
 *     Parent - Pointer to the parent entry, as returned by BmFindRegistryEntry() or by ourselves.
 *     Which - Ordinal of the entry to be found.
 *
 * RETURN VALUE:
 *     Pointer to a copy of the entry, or NULL if we failed to find it.
 *     The user is expected to manage and free it at the end of its lifetime.
 *-----------------------------------------------------------------------------------------------*/
RegEntryHeader *BmGetRegistryEntry(RegHandle *Handle, RegEntryHeader *Parent, int Which) {
    uint32_t KeyBlock = *(uint32_t *)((uint8_t *)Parent + Parent->Length - 4);
    RegBlockHeader *Header = (RegBlockHeader *)Handle->Buffer;
    RegEntryHeader *Current = NULL;

    while (1) {
        if (!Current || (char *)Current - Handle->Buffer >= REG_BLOCK_SIZE) {
            if (Current) {
                KeyBlock = Header->OffsetToNextBlock;
            }

            if (!KeyBlock ||
                BmReadFile(Handle->Stream, Handle->Buffer, KeyBlock, REG_BLOCK_SIZE, NULL) ||
                memcmp(Header->Signature, REG_BLOCK_SIGNATURE, 4)) {
                return NULL;
            }

            Current = (RegEntryHeader *)(Header + 1);
        }

        if (Current->Type == REG_ENTRY_REMOVED || Which--) {
            Current = (RegEntryHeader *)((char *)Current + Current->Length);
        } else {
            break;
        }
    }

    RegEntryHeader *Entry = BmAllocateBlock(Current->Length);

    if (Entry) {
        memcpy(Entry, Current, Current->Length);
    }

    return Entry;
}
