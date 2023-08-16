/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#include <registry.h>
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
