/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#include <file.h>
#include <memory.h>
#include <string.h>

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function implements the code to open a file, given an absolute path.
 *
 * PARAMETERS:
 *     File - Full path of the file to open.
 *
 * RETURN VALUE:
 *     File context if the file/device exists, NULL otherwise.
 *-----------------------------------------------------------------------------------------------*/
FileContext *BmOpenFile(const char *File) {
    char *Path = BmAllocateBlock(strlen(File) + 1);
    if (!Path) {
        return NULL;
    }

    strcpy(Path, File);

    FileContext *Context = BmAllocateZeroBlock(1, sizeof(FileContext));
    if (!Context) {
        BmFreeBlock(Path);
        return NULL;
    }

    for (char *Segment = strtok(Path, "/"); Segment; Segment = strtok(NULL, "/")) {
        /* First segment should always be the device. */
        if (Context->Type == FILE_TYPE_NONE) {
            int Offset = BiOpenConsoleDevice(Segment, Context);

            if (!Offset) {
                Offset = BiOpenArchDevice(Segment, Context);
            }

            if (!Offset) {
                BmFreeBlock(Context);
                return NULL;
            }

            Segment += Offset;
            if (*Segment) {
                BmCloseFile(Context);
                return NULL;
            }
        } else if (!BiReadDirectoryEntry(Context, Segment)) {
            BmCloseFile(Context);
            return NULL;
        }
    }

    return Context;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function closes the specified file handle.
 *     It will free the context handle, and the private data pointer (if not NULL).
 *
 * PARAMETERS:
 *     Context - Device/node private data.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void BmCloseFile(FileContext *Context) {
    if (Context->PrivateSize) {
        BmFreeBlock(Context->PrivateData);
    }

    BmFreeBlock(Context);
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function redirects the caller to the correct device-specific read function.
 *
 * PARAMETERS:
 *     Context - Device/node private data.
 *     Buffer - Output buffer.
 *     Start - Starting byte index (in the file).
 *     Size - How many bytes to read into the buffer.
 *     Read - How many bytes we read with no error.
 *
 * RETURN VALUE:
 *     1 if something went wrong, 0 otherwise.
 *-----------------------------------------------------------------------------------------------*/
int BmReadFile(FileContext *Context, void *Buffer, size_t Start, size_t Size, size_t *Read) {
    if (!Context || Context->Type == FILE_TYPE_NONE) {
        if (Read) {
            *Read = 0;
        }

        return 1;
    } else if (Context->Type == FILE_TYPE_CONSOLE) {
        return BiReadConsoleDevice(Context, Buffer, Start, Size, Read);
    } else if (Context->Type == FILE_TYPE_ARCH) {
        return BiReadArchDevice(Context, Buffer, Start, Size, Read);
    } else if (Context->Type == FILE_TYPE_EXFAT) {
        return BiReadExfatFile(Context, Buffer, Start, Size, Read);
    } else if (Context->Type == FILE_TYPE_FAT32) {
        return BiReadFat32File(Context, Buffer, Start, Size, Read);
    } else if (Context->Type == FILE_TYPE_ISO9660) {
        return BiReadIso9660File(Context, Buffer, Start, Size, Read);
    } else if (Context->Type == FILE_TYPE_NTFS) {
        return BiReadNtfsFile(Context, Buffer, Start, Size, Read);
    } else if (Read) {
        *Read = 0;
    }

    return 1;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function does a copy of the specified file handle, also allocating space for a copy of
 *     the private data pointer.
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
    }

    memcpy(Copy, Context, sizeof(FileContext));

    if (Context->PrivateSize) {
        Copy->PrivateData = BmAllocateBlock(Context->PrivateSize);

        if (!Copy->PrivateData) {
            return 0;
        }

        memcpy(Copy->PrivateData, Context->PrivateData, Context->PrivateSize);
    }

    return 1;
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
    } else if (Context->Type == FILE_TYPE_FAT32) {
        return BiTraverseFat32Directory(Context, Name);
    } else if (Context->Type == FILE_TYPE_ISO9660) {
        return BiTraverseIso9660Directory(Context, Name);
    } else if (Context->Type == FILE_TYPE_NTFS) {
        return BiTraverseNtfsDirectory(Context, Name);
    } else {
        return 0;
    }
}
