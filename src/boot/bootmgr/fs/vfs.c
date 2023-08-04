/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#include <assert.h>
#include <crt_impl.h>
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
void *__fopen(const char *filename, int mode) {
    (void)mode;

    char *Path = strdup(filename);
    if (!Path) {
        return NULL;
    }

    DeviceContext *Context = calloc(1, sizeof(DeviceContext));
    if (!Context) {
        free(Path);
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
                __fclose(Context);
                return NULL;
            }

            if (!BiProbeExfat(Context)) {
                __fclose(Context);
                return NULL;
            }

            continue;
        } else if (!BiReadDirectoryEntry(Context, Segment)) {
            __fclose(Context);
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
 *     handle - OS-specific handle; We expect/assume it is a DeviceContext.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void __fclose(void *handle) {
    DeviceContext *Context = handle;

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
 *     handle - OS-specific handle; We expect/assume it is a DeviceContext.
 *     pos - Offset (from the start of the file) to read the element from.
 *     buffer - Buffer to read the element into.
 *     size - Size of the element to read.
 *
 * RETURN VALUE:
 *     __STDIO_FLAGS_ERROR/EOF if something went wrong, 0 otherwise.
 *-----------------------------------------------------------------------------------------------*/
int __fread(void *handle, size_t pos, void *buffer, size_t size) {
    DeviceContext *Context = handle;

    if (!Context || Context->Type == DEVICE_TYPE_NONE) {
        return __STDIO_FLAGS_ERROR;
    } else if (Context->Type == DEVICE_TYPE_ARCH) {
        return BiReadArchDevice(Context, buffer, pos, size);
    } else if (Context->Type == DEVICE_TYPE_EXFAT) {
        return BiReadExfatFile(Context, buffer, pos, size);
    } else {
        return __STDIO_FLAGS_ERROR;
    }
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function redirects the caller to the correct device-specific write function.
 *
 * PARAMETERS:
 *     handle - OS-specific handle; We expect/assume it is a DeviceContext.
 *     pos - Offset (from the start of the file) to write the element into.
 *     buffer - Buffer to get the element from.
 *     size - Size of the element.
 *
 * RETURN VALUE:
 *     __STDIO_FLAGS_ERROR/EOF if something went wrong, 0 otherwise.
 *-----------------------------------------------------------------------------------------------*/
int __fwrite(void *handle, size_t pos, void *buffer, size_t size) {
    (void)handle;
    (void)pos;
    (void)buffer;
    (void)size;
    return __STDIO_FLAGS_ERROR;
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
