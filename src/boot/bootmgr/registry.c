/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#include <registry.h>
#include <stdio.h>
#include <stdlib.h>

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function loads a hive subkey/directory, such as the root entry.
 *
 * PARAMETERS:
 *     Stream - FILE stream pointing to the current location in the hive.
 *
 * RETURN VALUE:
 *     Pointer to the subkey, or NULL if we failed to load it.
 *-----------------------------------------------------------------------------------------------*/
static RegKey *LoadSubKey(FILE *Stream) {
    char *Name = calloc(32, sizeof(char));
    if (!Name) {
        return NULL;
    }

    size_t Position = 0;
    while (Position < 32) {
        int Character = fgetc(Stream);

        if (Character == EOF) {
            free(Name);
            return NULL;
        } else if (!Character) {
            break;
        }

        Name[Position++] = Character;
    }

    /* If we don't have space for the NULL terminator, then we can be certain that this is an
       invalid file. */
    if (Position >= 32) {
        free(Name);
        return NULL;
    }

    uint32_t EntryCount;
    if (fread(&EntryCount, sizeof(uint32_t), 1, Stream) != 1) {
        free(Name);
        return NULL;
    }

    RegKey *Hive = malloc(sizeof(RegKey));
    if (!Hive) {
        free(Name);
        return NULL;
    }

    Hive->Type = REG_SUBKEY;
    Hive->SubKey.Name = Name;
    Hive->SubKey.EntryCount = EntryCount;
    Hive->SubKey.Entries = malloc(EntryCount * sizeof(RegKey));
    if (!Hive->SubKey.Entries) {
        free(Hive);
        free(Name);
        return NULL;
    }

    /* There should be no `END` marker/node, as the EntryCount already tells us how far away
       we need to/should go. */
    while (EntryCount--) {
    }

    return Hive;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function loads the registry hive at `Path`, returning the hive root entry.
 *
 * PARAMETERS:
 *     Path - Full path to the registry hive file.
 *
 * RETURN VALUE:
 *     Pointer to the root key, or NULL if we failed to load it.
 *-----------------------------------------------------------------------------------------------*/
RegKey *BmLoadHive(const char *Path) {
    FILE *Stream = fopen(Path, "rb");
    if (!Stream) {
        return NULL;
    }

    /* Every hive file is guaranteed to start with the HIVE signature, followed by the hive
       "name"/nickname, and how many entries we have; The name MUST have at most 32 bytes
       (including the NULL-terminator). */
    if (fgetc(Stream) != 'H' || fgetc(Stream) != 'I' || fgetc(Stream) != 'V' ||
        fgetc(Stream) != 'E') {
        fclose(Stream);
        return NULL;
    }

    /* From now on, we can use the common entry point (LoadSubKey), as the format of the root
       entry, and of other sub keys/directories is the exact same. */
    RegKey *Hive = LoadSubKey(Stream);
    if (!Hive) {
        free(Stream);
    }

    return Hive;
}
