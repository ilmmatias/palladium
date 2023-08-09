/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#include <assert.h>
#include <crt_impl.h>
#include <file.h>
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

    FileContext *Context = calloc(1, sizeof(FileContext));
    if (!Context) {
        free(Path);
        return NULL;
    }

    for (char *Segment = strtok(Path, "/"); Segment; Segment = strtok(NULL, "/")) {
        /* First segment should be device; hard-coded to OpenArchDevice, but later on we should
           fall back to at last `display()` too. */
        if (Context->Type == FILE_TYPE_NONE) {
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
 *     handle - OS-specific handle; We expect/assume it is a FileContext.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void __fclose(void *handle) {
    FileContext *Context = handle;

    if (!Context || Context->Type == FILE_TYPE_NONE) {
        return;
    } else if (Context->Type == FILE_TYPE_ARCH) {
        BiFreeArchDevice(Context);
    } else if (Context->Type == FILE_TYPE_EXFAT) {
        BiCleanupExfat(Context);
    } else if (Context->Type == FILE_TYPE_NTFS) {
        BiCleanupNtfs(Context);
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
 *     read - How many bytes of the total we were able to read.
 *
 * RETURN VALUE:
 *     __STDIO_FLAGS_ERROR/EOF if something went wrong, 0 otherwise.
 *-----------------------------------------------------------------------------------------------*/
int __fread(void *handle, size_t pos, void *buffer, size_t size, size_t *read) {
    FileContext *Context = handle;

    if (!Context || Context->Type == FILE_TYPE_NONE) {
        if (read) {
            *read = 0;
        }

        return __STDIO_FLAGS_ERROR;
    } else if (Context->Type == FILE_TYPE_ARCH) {
        return BiReadArchDevice(Context, buffer, pos, size, read);
    } else if (Context->Type == FILE_TYPE_EXFAT) {
        return BiReadExfatFile(Context, buffer, pos, size, read);
    } else if (Context->Type == FILE_TYPE_NTFS) {
        return BiReadNtfsFile(Context, buffer, pos, size, read);
    } else {
        if (read) {
            *read = 0;
        }

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
 *     wrote - How many bytes of the total we were able to write.
 *
 * RETURN VALUE:
 *     __STDIO_FLAGS_ERROR/EOF if something went wrong, 0 otherwise.
 *-----------------------------------------------------------------------------------------------*/
int __fwrite(void *handle, size_t pos, void *buffer, size_t size, size_t *wrote) {
    (void)handle;
    (void)pos;
    (void)buffer;
    (void)size;

    if (wrote) {
        *wrote = 0;
    }

    return __STDIO_FLAGS_ERROR;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function redirects the caller to the correct device-specific skeleton clone function.
 *
 * PARAMETERS:
 *     Context - Device/node private data.
 *     Copy - Destination of the copy.
 *
 * RETURN VALUE:
 *     1 for success, otherwise 0.
 *-----------------------------------------------------------------------------------------------*/
int BiCopyFileContext(FileContext *Context, FileContext *Copy) {
    if (!Context) {
        return 0;
    } else if (Context->Type == FILE_TYPE_NONE) {
        memcpy(Copy, Context, sizeof(FileContext));
        return 1;
    } else if (Context->Type == FILE_TYPE_ARCH) {
        return BiCopyArchDevice(Context, Copy);
    } else if (Context->Type == FILE_TYPE_EXFAT) {
        return BiCopyExfat(Context, Copy);
    } else if (Context->Type == FILE_TYPE_NTFS) {
        return BiCopyNtfs(Context, Copy);
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
int BiReadDirectoryEntry(FileContext *Context, const char *Name) {
    if (!Context || Context->Type == FILE_TYPE_NONE) {
        return 0;
    } else if (Context->Type == FILE_TYPE_ARCH) {
        return BiReadArchDirectoryEntry(Context, Name);
    } else if (Context->Type == FILE_TYPE_EXFAT) {
        return BiTraverseExfatDirectory(Context, Name);
    } else if (Context->Type == FILE_TYPE_NTFS) {
        return BiTraverseNtfsDirectory(Context, Name);
    } else {
        return 0;
    }
}
