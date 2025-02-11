/* SPDX-FileCopyrightText: (C) 2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <kernel/ke.h>
#include <kernel/mm.h>
#include <kernel/obp.h>
#include <rt/hash.h>
#include <string.h>

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
    /* No references are left, and as such, no one else should be able to hold the lock, so we'll
     * skip on acquiring it. */
    ObDirectory *Directory = Object;
    for (size_t Head = 0; Head < 32; Head++) {
        while (Directory->HashHeads[Head].Next != &Directory->HashHeads[Head]) {
            RtDList *ListHeader = RtPopDList(&Directory->HashHeads[Head]);
            ObpDirectoryEntry *Entry = CONTAINING_RECORD(ListHeader, ObpDirectoryEntry, HashHeader);

            /* Take caution if someone is deleting a directory an object is contained at the same
             * time someone else is trying to unlink said object. */
            ObpObject *ObjectHeader = (ObpObject *)Entry->Object - 1;
            ObpDirectoryEntry *ExpectedParent = Entry;
            if (__atomic_compare_exchange_n(
                    &ObjectHeader->Parent,
                    &ExpectedParent,
                    NULL,
                    0,
                    __ATOMIC_ACQUIRE,
                    __ATOMIC_RELAXED)) {
                ObDereferenceObject(Entry->Object);
                MmFreePool(Entry->Name, MM_POOL_TAG_OBJECT);
                MmFreePool(Entry, MM_POOL_TAG_OBJECT);
            }
        }
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
void ObInitializeRootDirectory(void) {
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
    /* Don't even bother with anything if this object is already linked to something else. */
    ObpObject *ObjectHeader = (ObpObject *)Object - 1;
    if (ObjectHeader->Parent) {
        return false;
    }

    /* Otherwise, allocate all memory we need for the dir entry+its name. */
    ObpDirectoryEntry *Entry = MmAllocatePool(sizeof(ObpDirectoryEntry), MM_POOL_TAG_OBJECT);
    if (!Entry) {
        return false;
    }

    size_t NameSize = strlen(Name);
    Entry->Name = MmAllocatePool(NameSize + 1, MM_POOL_TAG_OBJECT);
    if (!Entry->Name) {
        MmFreePool(Entry, MM_POOL_TAG_OBJECT);
        return false;
    }

    /* Hopefully this will succeed, otherwise, free up everything and bail out. */
    ObpDirectoryEntry *ExpectedParent = NULL;
    if (!__atomic_compare_exchange_n(
            &ObjectHeader->Parent, &ExpectedParent, Entry, 0, __ATOMIC_ACQUIRE, __ATOMIC_RELAXED)) {
        MmFreePool(Entry->Name, MM_POOL_TAG_OBJECT);
        MmFreePool(Entry, MM_POOL_TAG_OBJECT);
        return false;
    }

    /* Now we're past the last place we could have gotten an allocation failure, so, block
     * the object from dying after we return. */
    Entry->Object = Object;
    Entry->Parent = Directory;
    memcpy(Entry->Name, Name, NameSize);
    ObReferenceObject(Object);

    /* And append to the directory. With this, we should be done. */
    uint32_t Hash = RtGetHash(Name, NameSize);
    KeIrql OldIrql = KeAcquireSpinLockAndRaiseIrql(&Directory->Lock, KE_IRQL_DISPATCH);
    RtAppendDList(&Directory->HashHeads[Hash & 0x1F], &Entry->HashHeader);
    KeReleaseSpinLockAndLowerIrql(&Directory->Lock, OldIrql);
    return true;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function removes the specified object from its current directory.
 *
 * PARAMETERS:
 *     Object - Which object to be removed from its parent.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void ObRemoveFromDirectory(void *Object) {
    ObpObject *ObjectHeader = (ObpObject *)Object - 1;
    ObpDirectoryEntry *DirectoryEntry = ObjectHeader->Parent;
    if (!DirectoryEntry) {
        return;
    }

    /* As long as we do an interlocked xchg, this should work... */
    ObpDirectoryEntry *ExpectedParent = DirectoryEntry;
    if (!__atomic_compare_exchange_n(
            &ObjectHeader->Parent, &ExpectedParent, NULL, 0, __ATOMIC_ACQUIRE, __ATOMIC_RELAXED)) {
        return;
    }

    /* Lock the parent and unlink from it. */
    ObDirectory *Directory = DirectoryEntry->Parent;
    KeIrql OldIrql = KeAcquireSpinLockAndRaiseIrql(&Directory->Lock, KE_IRQL_DISPATCH);
    RtUnlinkDList(&DirectoryEntry->HashHeader);
    KeReleaseSpinLockAndLowerIrql(&Directory->Lock, OldIrql);

    /* Now just cleanup everything before returning. */
    ObDereferenceObject(DirectoryEntry->Object);
    MmFreePool(DirectoryEntry->Name, MM_POOL_TAG_OBJECT);
    MmFreePool(DirectoryEntry, MM_POOL_TAG_OBJECT);
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
 *     Name - A pointer to store the name of the entry into, or NULL if the name shouldn't
 *            be saved.
 *
 * RETURN VALUE:
 *     Either a pointer to the object if found, or NULL otherwise.
 *-----------------------------------------------------------------------------------------------*/
void *ObLookupDirectoryEntryByIndex(ObDirectory *Directory, size_t Index, char **Name) {
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

            /* Only save the name if we were requested to do so. */
            if (Name) {
                *Name = Entry->Name;
            }

            KeReleaseSpinLockAndLowerIrql(&Directory->Lock, OldIrql);
            return Entry->Object;
        }
    }

    KeReleaseSpinLockAndLowerIrql(&Directory->Lock, OldIrql);
    return NULL;
}
