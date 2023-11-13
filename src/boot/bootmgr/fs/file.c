/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#include <ctype.h>
#include <file.h>
#include <memory.h>
#include <string.h>

extern BmPartition *BiBootPartition;

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function implements the code to open a file handle, given an absolute path.
 *
 * PARAMETERS:
 *     File - Full path of the file to open.
 *
 * RETURN VALUE:
 *     Handle if the file/device exists, NULL otherwise.
 *-----------------------------------------------------------------------------------------------*/
BmFile *BmOpenFile(const char *File) {
    /* strtok() modifies the string we pass to it. */
    char *Path = BmAllocateBlock(strlen(File) + 1);
    if (!Path) {
        return NULL;
    }

    strcpy(Path, File);

    BmFile *Handle = NULL;
    int RawDevice = 1;

    for (char *Segment = strtok(Path, "/"); Segment; Segment = strtok(NULL, "/")) {
        if (!Handle) {
            /* Initial pass, we need to open either a disk/platform device, or the boot partition;
               We know which by looking at the start of the segment, boot partitions always start
               with `boot()`. */
            if (tolower(*Segment) == 'b' && tolower(*(Segment + 1)) == 'o' &&
                tolower(*(Segment + 2)) == 'o' && tolower(*(Segment + 3)) == 't' &&
                tolower(*(Segment + 4)) == '(') {
                if (*(Segment + 5) != ')') {
                    BmFreeBlock(Path);
                    return NULL;
                }

                Handle = BiOpenBootPartition();
            } else {
                RtSList *ListHead;
                Handle = BiOpenDevice(&Segment, &ListHead);

                if (Handle && *Segment) {
                    BmCloseFile(Handle);
                    Handle = BiOpenPartition(ListHead, Segment);
                }
            }

            if (!Handle) {
                BmFreeBlock(Path);
                return NULL;
            }

            continue;
        }

        if (RawDevice) {
            /* It should be safe to use the context after Close(). */
            BmPartition *Partition = Handle->Context;
            BmCloseFile(Handle);

            Handle = BiOpenRoot(Partition);
            if (!Handle) {
                BmFreeBlock(Path);
                return NULL;
            }

            RawDevice = 0;
        }

        BmFile *NewHandle = BmReadDirectoryEntry(Handle, Segment);
        BmCloseFile(Handle);
        if (!NewHandle) {
            BmFreeBlock(Path);
            return NULL;
        }

        Handle = NewHandle;
    }

    BmFreeBlock(Path);
    return Handle;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function closes the specified file handle.
 *
 * PARAMETERS:
 *     File - File/directory handle.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void BmCloseFile(BmFile *File) {
    if (File->Close) {
        File->Close(File->Context);
    }

    BmFreeBlock(File);
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function reads the requested amount of bytes from the file.
 *
 * PARAMETERS:
 *     File - File handle.
 *     Offset - Starting byte index (in the file).
 *     Size - How many bytes to read into the buffer.
 *     Buffer - Output buffer.
 *
 * RETURN VALUE:
 *     0 if something went wrong, 1 otherwise.
 *-----------------------------------------------------------------------------------------------*/
int BmReadFile(BmFile *File, uint64_t Offset, uint64_t Size, void *Buffer) {
    return File->Read ? File->Read(File->Context, Offset, Size, Buffer) : 0;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function reads the requested file/entry inside a directory.
 *
 * PARAMETERS:
 *     Directory - Directory handle.
 *     Name - Name of the entry.
 *
 * RETURN VALUE:
 *     Handle to the open entry on success, NULL otherwise.
 *-----------------------------------------------------------------------------------------------*/
BmFile *BmReadDirectoryEntry(BmFile *Directory, const char *Name) {
    return Directory->ReadEntry ? Directory->ReadEntry(Directory->Context, Name) : 0;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function iterates over the given directory, returning the Index-th entry in it.
 *
 * PARAMETERS:
 *     Directory - Directory handle.
 *     Index - Which entry to return.
 *
 * RETURN VALUE:
 *     Name of the entry on success, NULL otherwise.
 *-----------------------------------------------------------------------------------------------*/
char *BmIterateDirectory(BmFile *Directory, int Index) {
    return Directory->Iterate ? Directory->Iterate(Directory->Context, Index) : NULL;
}
