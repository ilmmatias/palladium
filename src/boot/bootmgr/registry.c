/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#include <registry.h>
#include <rt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

    FILE *Stream = fopen(Path, "r");
    if (!Stream) {
        return NULL;
    }

    RegHandle *Handle = malloc(sizeof(RegHandle));
    if (!Handle) {
        fclose(Stream);
        return NULL;
    } else if (fread(Handle->Buffer, REG_BLOCK_SIZE, 1, Stream) != 1) {
        free(Handle);
        fclose(Stream);
        return NULL;
    }

    RegFileHeader *FileHeader = (RegFileHeader *)Handle->Buffer;
    if (memcmp(FileHeader->Signature, REG_FILE_SIGNATURE, 4)) {
        free(Handle);
        fclose(Stream);
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

            if (!KeyBlock || fseek(Handle->Stream, KeyBlock, SEEK_SET) ||
                fread(Handle->Buffer, REG_BLOCK_SIZE, 1, Handle->Stream) != 1 ||
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
 *     Path - Full path to the key or value.
 *
 * RETURN VALUE:
 *     Pointer to a copy of the entry, or NULL if we failed to find it.
 *     The user is expected to manage and free it at the end of its lifetime.
 *-----------------------------------------------------------------------------------------------*/
RegEntryHeader *BmFindRegistryEntry(RegHandle *Handle, const char *Path) {
    char *CopyPath = strdup(Path);
    if (!CopyPath) {
        return NULL;
    }

    RegEntryHeader *Current = NULL;
    for (char *Segment = strtok(CopyPath, "/"); Segment; Segment = strtok(NULL, "/")) {
        if (Current) {
            if (Current->Type != REG_ENTRY_SUBKEY) {
                return NULL;
            }

            size_t NameLength = strlen((const char *)(Current + 1)) + 1;
            uint32_t *EntryData =
                (uint32_t *)((uint8_t *)Current + sizeof(RegEntryHeader) + NameLength);

            Current = FindEntry(Handle, *EntryData, Segment);
        } else {
            Current = FindEntry(Handle, sizeof(RegFileHeader), Segment);
        }

        if (!Current) {
            return NULL;
        }
    }

    /* The buffer where the entry is currently located might be reused for loading other entries,
       so we'll be copying it to a new buffer (managed by the caller). */
    RegEntryHeader *Entry = malloc(Current ? Current->Length : sizeof(RegEntryHeader) + 5);

    if (!Entry) {
        return NULL;
    } else if (Current) {
        memcpy(Entry, Current, Current->Length);
    } else {
        Entry->Type = REG_ENTRY_SUBKEY;
        Entry->Length = sizeof(RegEntryHeader) + 5;
        Entry->NameHash = 0;
        *((char *)(Entry + 1)) = 0;
        *((uint32_t *)((uint8_t *)Entry + sizeof(RegEntryHeader) + 1)) = sizeof(RegFileHeader);
    }

    return Entry;
}
