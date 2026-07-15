/* SPDX-FileCopyrightText: (C) 2025-2026 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <kernel/ke.h>
#include <kernel/mm.h>
#include <kernel/ob.h>
#include <kernel/obp.h>
#include <os/containing_record.h>
#include <rt/hash.h>
#include <rt/list.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

static KeSpinLock TreeLock = {0};

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function checks whether inserting a directory below another directory would create an
 *     ancestor cycle. The directory tree lock must already be held.
 *
 * PARAMETERS:
 *     Ancestor - Directory proposed as the child object.
 *     Directory - Proposed parent directory.
 *
 * RETURN VALUE:
 *     true if Ancestor is Directory or already appears above it in the directory tree.
 *-----------------------------------------------------------------------------------------------*/
static bool IsAncestorDirectory(ObDirectory *Ancestor, ObDirectory *Directory) {
    ObDirectory *Current = Directory;
    while (Current) {
        if (Current == Ancestor) {
            return true;
        }

        if (Current == &ObRootDirectory) {
            break;
        }

        ObpObject *CurrentHeader = (ObpObject *)Current - 1;
        Current = CurrentHeader->Parent ? CurrentHeader->Parent->Parent : NULL;
    }

    return false;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function cleans up a directory object after all references to it have been removed.
 *
 * PARAMETERS:
 *     Object - Which object to clean up.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
static void DeleteRoutine(void *Object) {
    ObDirectory *Directory = Object;
    RtDList DetachedEntries;
    RtInitializeDList(&DetachedEntries);

    KeIrql OldIrql = KeAcquireSpinLockAndRaiseIrql(&TreeLock, KE_IRQL_DISPATCH);
    KeAcquireSpinLockAtCurrentIrql(&Directory->Lock);

    for (size_t Head = 0; Head < 32; Head++) {
        while (Directory->HashHeads[Head].Next != &Directory->HashHeads[Head]) {
            RtDList *ListHeader = RtPopDList(&Directory->HashHeads[Head]);
            ObpDirectoryEntry *Entry = CONTAINING_RECORD(ListHeader, ObpDirectoryEntry, HashHeader);
            ObpObject *ObjectHeader = (ObpObject *)Entry->Object - 1;
            if (ObjectHeader->Parent != Entry) {
                KeFatalError(
                    KE_PANIC_BAD_OBJECT_REFERENCE_COUNT,
                    (uint64_t)Entry->Object,
                    (uint64_t)ObjectHeader->Parent,
                    (uint64_t)Entry,
                    2);
            }

            ObjectHeader->Parent = NULL;
            RtAppendDList(&DetachedEntries, &Entry->HashHeader);
        }
    }

    KeReleaseSpinLockAtCurrentIrql(&Directory->Lock);
    KeReleaseSpinLockAndLowerIrql(&TreeLock, OldIrql);

    while (DetachedEntries.Next != &DetachedEntries) {
        ObpDirectoryEntry *Entry =
            CONTAINING_RECORD(RtPopDList(&DetachedEntries), ObpDirectoryEntry, HashHeader);
        ObDereferenceObject(Entry->Object);
        MmFreePool(Entry->Name, MM_POOL_TAG_OBJECT);
        MmFreePool(Entry, MM_POOL_TAG_OBJECT);
    }
}

ObType ObpDirectoryType = {
    .Name = "Directory",
    .Size = sizeof(ObDirectory),
    .Delete = DeleteRoutine,
};

ObDirectory ObRootDirectory;

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function initializes the root directory object.
 *
 * PARAMETERS:
 *     None.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void ObpInitializeRootDirectory(void) {
    for (size_t i = 0; i < 32; i++) {
        RtInitializeDList(&ObRootDirectory.HashHeads[i]);
    }
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function creates and initializes a new object directory.
 *
 * PARAMETERS:
 *     None.
 *
 * RETURN VALUE:
 *     Pointer to the allocated object directory, or NULL on failure.
 *-----------------------------------------------------------------------------------------------*/
ObDirectory *ObCreateDirectory(void) {
    /* There's not much we really have to do here... */
    ObDirectory *Directory = ObCreateObject(&ObpDirectoryType, MM_POOL_TAG_OBJECT);
    if (Directory) {
        for (size_t i = 0; i < 32; i++) {
            RtInitializeDList(&Directory->HashHeads[i]);
        }
    }

    return Directory;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function inserts the specified object inside the specified directory.
 *
 * PARAMETERS:
 *     Directory - Target directory to insert the object.
 *     Name - Name of the object to be inserted.
 *     Object - Which object to insert.
 *
 * RETURN VALUE:
 *     true if the insertion was successful, false otherwise.
 *-----------------------------------------------------------------------------------------------*/
bool ObInsertIntoDirectory(ObDirectory *Directory, const char *Name, void *Object) {
    ObpObject *ObjectHeader = (ObpObject *)Object - 1;
    ObpDirectoryEntry *Entry = MmAllocatePool(sizeof(ObpDirectoryEntry), MM_POOL_TAG_OBJECT);
    if (!Entry) {
        return false;
    }

    size_t NameSize = strlen(Name);
    if (NameSize == (size_t)-1) {
        MmFreePool(Entry, MM_POOL_TAG_OBJECT);
        return false;
    }

    Entry->Name = MmAllocatePool(NameSize + 1, MM_POOL_TAG_OBJECT);
    if (!Entry->Name) {
        MmFreePool(Entry, MM_POOL_TAG_OBJECT);
        return false;
    }

    /* Initialize the entry fields. */
    Entry->Object = Object;
    Entry->Parent = Directory;
    memcpy(Entry->Name, Name, NameSize);
    Entry->Name[NameSize] = 0;
    RtInitializeDList(&Entry->HashHeader);

    /* Let's try locking up the head we need so that no one competes with us... */
    uint32_t Hash = RtGetHash(Name, NameSize);
    RtDList *Bucket = &Directory->HashHeads[Hash & 0x1F];
    bool Inserted = false;
    KeIrql OldIrql = KeAcquireSpinLockAndRaiseIrql(&TreeLock, KE_IRQL_DISPATCH);
    KeAcquireSpinLockAtCurrentIrql(&Directory->Lock);

    /* Then validate sanity (no duplicates + no cycles), and we can insert. */
    bool Duplicate = false;
    for (RtDList *ListHeader = Bucket->Next; ListHeader != Bucket; ListHeader = ListHeader->Next) {
        ObpDirectoryEntry *OtherEntry =
            CONTAINING_RECORD(ListHeader, ObpDirectoryEntry, HashHeader);
        if (!strcmp(OtherEntry->Name, Name)) {
            Duplicate = true;
            break;
        }
    }

    bool Cycle = ObjectHeader->Type == &ObpDirectoryType &&
                 IsAncestorDirectory((ObDirectory *)Object, Directory);
    if (!ObjectHeader->Parent && !Duplicate && !Cycle) {
        ObReferenceObject(Object);
        ObjectHeader->Parent = Entry;
        RtAppendDList(Bucket, &Entry->HashHeader);
        Inserted = true;
    }

    KeReleaseSpinLockAtCurrentIrql(&Directory->Lock);
    KeReleaseSpinLockAndLowerIrql(&TreeLock, OldIrql);
    if (!Inserted) {
        MmFreePool(Entry->Name, MM_POOL_TAG_OBJECT);
        MmFreePool(Entry, MM_POOL_TAG_OBJECT);
    }

    return Inserted;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function removes the specified object from its current directory.
 *
 * PARAMETERS:
 *     Directory - Referenced parent directory.
 *     Object - Referenced object to remove from Directory.
 *
 * RETURN VALUE:
 *     true if the object was removed, false if it was not a member of Directory.
 *-----------------------------------------------------------------------------------------------*/
bool ObRemoveFromDirectory(ObDirectory *Directory, void *Object) {
    ObpObject *ObjectHeader = (ObpObject *)Object - 1;
    ObpDirectoryEntry *DirectoryEntry = NULL;

    /* Just make sure we're not freeing any object that isn't properly part of the specified
     * directory. */
    KeIrql OldIrql = KeAcquireSpinLockAndRaiseIrql(&TreeLock, KE_IRQL_DISPATCH);
    KeAcquireSpinLockAtCurrentIrql(&Directory->Lock);
    if (ObjectHeader->Parent && ObjectHeader->Parent->Parent == Directory) {
        DirectoryEntry = ObjectHeader->Parent;
        ObjectHeader->Parent = NULL;
        RtUnlinkDList(&DirectoryEntry->HashHeader);
    }

    KeReleaseSpinLockAtCurrentIrql(&Directory->Lock);
    KeReleaseSpinLockAndLowerIrql(&TreeLock, OldIrql);
    if (!DirectoryEntry) {
        return false;
    }

    /* Now just cleanup everything before returning. */
    ObDereferenceObject(DirectoryEntry->Object);
    MmFreePool(DirectoryEntry->Name, MM_POOL_TAG_OBJECT);
    MmFreePool(DirectoryEntry, MM_POOL_TAG_OBJECT);
    return true;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function searches for an entry with the given name inside the specified directory.
 *
 * PARAMETERS:
 *     Directory - Target directory for the search.
 *     Name - Name of the object to be searched for.
 *
 * RETURN VALUE:
 *     Either a pointer to the object if found, or NULL otherwise.
 *-----------------------------------------------------------------------------------------------*/
void *ObLookupDirectoryEntryByName(ObDirectory *Directory, const char *Name) {
    /* Lock up the directory, and just search directly on the bucket the name is contained. */
    uint32_t Hash = RtGetHash(Name, strlen(Name));
    RtDList *Bucket = &Directory->HashHeads[Hash & 0x1F];
    KeIrql OldIrql = KeAcquireSpinLockAndRaiseIrql(&Directory->Lock, KE_IRQL_DISPATCH);

    for (RtDList *ListHeader = Bucket->Next; ListHeader != Bucket; ListHeader = ListHeader->Next) {
        ObpDirectoryEntry *Entry = CONTAINING_RECORD(ListHeader, ObpDirectoryEntry, HashHeader);
        if (!strcmp(Entry->Name, Name)) {
            ObReferenceObject(Entry->Object);
            KeReleaseSpinLockAndLowerIrql(&Directory->Lock, OldIrql);
            return Entry->Object;
        }
    }

    KeReleaseSpinLockAndLowerIrql(&Directory->Lock, OldIrql);
    return NULL;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function searches for an entry with the given index inside the specified
 *     directory.
 *
 * PARAMETERS:
 *     Directory - Target directory for the search.
 *     Index - Which index to look for.
 *     Name - Caller-supplied name buffer (NULL if you don't want to save the name).
 *     NameSize - Size of Name in bytes.
 *     NameCapacity - Where to return the required name size (when we don't have enough space in
 *                    NameSize).
 *
 * RETURN VALUE:
 *     Either a pointer to the object if found, or NULL on failure.
 *-----------------------------------------------------------------------------------------------*/
void *ObLookupDirectoryEntryByIndex(
    ObDirectory *Directory,
    size_t Index,
    char *Name,
    size_t NameSize,
    size_t *NameCapacity) {
    /* Don't even bother if we can't write the NameCapacity (when the Name was requested). */
    if (Name && !NameCapacity) {
        return NULL;
    }

    if (NameCapacity) {
        *NameCapacity = 0;
    }

    KeIrql OldIrql = KeAcquireSpinLockAndRaiseIrql(&Directory->Lock, KE_IRQL_DISPATCH);

    /* Here we don't know which bucket the index is gonna fall, so we gotta iterate
     * entry-by-entry in each hash bucket. */
    size_t CurrentIndex = 0;
    for (size_t Head = 0; Head < 32; Head++) {
        for (RtDList *ListHeader = Directory->HashHeads[Head].Next;
             ListHeader != &Directory->HashHeads[Head];
             ListHeader = ListHeader->Next) {
            ObpDirectoryEntry *Entry = CONTAINING_RECORD(ListHeader, ObpDirectoryEntry, HashHeader);
            if (CurrentIndex++ != Index) {
                continue;
            }

            /* Make sure we return the proper combo (NULL alongside *NameCapacity != 0) if we found
             * it but don't have enough space to save the name. */
            if (Name) {
                size_t EntryNameSize = strlen(Entry->Name) + 1;
                *NameCapacity = EntryNameSize;
                if (NameSize < EntryNameSize) {
                    KeReleaseSpinLockAndLowerIrql(&Directory->Lock, OldIrql);
                    return NULL;
                }

                memcpy(Name, Entry->Name, EntryNameSize);
            }

            ObReferenceObject(Entry->Object);
            KeReleaseSpinLockAndLowerIrql(&Directory->Lock, OldIrql);
            return Entry->Object;
        }
    }

    KeReleaseSpinLockAndLowerIrql(&Directory->Lock, OldIrql);
    return NULL;
}
