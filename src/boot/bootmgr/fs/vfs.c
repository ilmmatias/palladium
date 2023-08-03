/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#include <assert.h>
#include <device.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function implements the code to open a file, returning a handle we can use (and the
 *     CRT can transparently store).
 *
 * PARAMETERS:
 *     path - Full path of the file to open.
 *     mode - File flags; We don't really have any use for them, and they are ignore.
 *
 * RETURN VALUE:
 *     Transparent handle if the file/device exists, NULL otherwise.
 *-----------------------------------------------------------------------------------------------*/
void *__open(const char *path, int mode) {
    (void)mode;

    char *Path = strdup(path);
    if (!Path) {
        return NULL;
    }

    DeviceContext *Context = calloc(1, sizeof(DeviceContext));
    if (!Context) {
        return NULL;
    }

    for (char *Segment = strtok(Path, "/"); Segment; Segment = strtok(NULL, "/")) {
        /* First segment should be device; hard-coded to OpenArchDevice, but later on we should
           fall back to at last `display()` too. */
        if (Context->Type == DEVICE_TYPE_NONE) {
            int Offset = BiOpenArchDevice(Segment, Context);
            if (!Offset) {
                free(Context);
                return NULL;
            }

            Segment += Offset;
            if (*Segment) {
                BiFreeDevice(Context);
                return NULL;
            }

            if (!BiProbeExfat(Context)) {
                BiFreeDevice(Context);
                return NULL;
            }

            continue;
        } else if (!BiReadDirectoryEntry(Context, Segment)) {
            BiFreeDevice(Context);
            return NULL;
        }
    }

    return Context;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function redirects the caller to the correct device-specific free function.
 *     We expect it to also free the context itself.
 *
 * PARAMETERS:
 *     Context - Device/node private data.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void BiFreeDevice(DeviceContext *Context) {
    if (!Context || Context->Type == DEVICE_TYPE_NONE) {
        return;
    } else if (Context->Type == DEVICE_TYPE_ARCH) {
        BiFreeArchDevice(Context);
    } else if (Context->Type == DEVICE_TYPE_EXFAT) {
        BiCleanupExfat(Context);
    }
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function redirects the caller to the correct device-specific read function.
 *
 * PARAMETERS:
 *     Context - Device/node private data.
 *     Buffer - Output buffer.
 *     Start - Starting byte index (in the device).
 *     Size - How many bytes to read into the buffer.
 *
 * RETURN VALUE:
 *     1 for success, otherwise 0.
 *-----------------------------------------------------------------------------------------------*/
int BiReadDevice(DeviceContext *Context, void *Buffer, uint64_t Start, size_t Size) {
    if (!Context || Context->Type == DEVICE_TYPE_NONE) {
        return 0;
    } else if (Context->Type == DEVICE_TYPE_ARCH) {
        return BiReadArchDevice(Context, Buffer, Start, Size);
    } else {
        return 0;
    }
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function redirects the caller to the correct device-specific directory travesal
 *     function.
 *
 * PARAMETERS:
 *     Context - Device/node private data.
 *     Name - What entry to find inside the directory.
 *
 * RETURN VALUE:
 *     1 for success, otherwise 0.
 *-----------------------------------------------------------------------------------------------*/
int BiReadDirectoryEntry(DeviceContext *Context, const char *Name) {
    if (!Context || Context->Type == DEVICE_TYPE_NONE) {
        return 0;
    } else if (Context->Type == DEVICE_TYPE_ARCH) {
        return BiReadArchDirectoryEntry(Context, Name);
    } else if (Context->Type == DEVICE_TYPE_EXFAT) {
        return BiTraverseExfatDirectory(Context, Name);
    } else {
        return 0;
    }
}
