/* SPDX-FileCopyrightText: (C) 2023-2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <crt_impl.h>
#include <string.h>

typedef struct allocator_entry_t {
    int used;
    size_t size;
    struct allocator_entry_t *prev, *next;
} allocator_entry_t;

static allocator_entry_t *head = NULL, *tail = NULL;

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function splits a block of memory into two blocks, one of the required size for the
 *     allocation and one of the remaining size.
 *
 * PARAMETERS:
 *     entry - The entry to split.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
static void split_entry(allocator_entry_t *entry, size_t size) {
    if (entry->size <= size + sizeof(allocator_entry_t)) {
        return;
    }

    allocator_entry_t *new_entry =
        (allocator_entry_t *)((uintptr_t)entry + sizeof(allocator_entry_t) + size);

    new_entry->used = 0;
    new_entry->size = entry->size - (size + sizeof(allocator_entry_t));
    new_entry->prev = entry;
    new_entry->next = NULL;

    tail = new_entry;

    entry->size = size;
    entry->next = new_entry;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function merges all contiguous free entries after and including the base entry into
 *     one entry.
 *
 * PARAMETERS:
 *     base - The entry to start merging from.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
static void merge_forward(allocator_entry_t *base) {
    while (base->next && base + (base->size << 12) == base->next && !base->next->used) {
        base->size += base->next->size;
        base->next = base->next->next;

        if (base->next) {
            base->next->prev = base;
        }
    }

    if (!base->next) {
        tail = base;
    }
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function merges all contiguous free entries before and including the base entry into
 *     one entry.
 *
 * PARAMETERS:
 *     base - The entry to start merging from.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
static void merge_backward(allocator_entry_t *base) {
    while (base->prev && base->prev + (base->prev->size << 12) == base && !base->prev->used) {
        base->prev->size += base->size;
        base->prev->next = base->next;

        if (base->next) {
            base->next->prev = base->prev;
        }

        base = base->prev;
    }

    if (!base->prev) {
        head = base;
    }

    if (!base->next) {
        tail = base;
    }
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function finds a free entry of the specified size, or creates requests a page
 *     and uses it to create a new entry.
 *
 * PARAMETERS:
 *     size - The size of the entry to find or create.
 *
 * RETURN VALUE:
 *     A pointer to the allocated entry, or NULL if there is no more free regions in the memory
 *     map.
 *-----------------------------------------------------------------------------------------------*/
static allocator_entry_t *find_free(size_t size) {
    allocator_entry_t *entry = head;
    size_t mask = __PAGE_SIZE - 1;

    while (entry) {
        if (!entry->used && entry->size >= size) {
            entry->used = 1;
            return entry;
        }

        entry = entry->next;
    }

    entry = __allocate_pages((size + sizeof(allocator_entry_t) + mask) >> __PAGE_SHIFT);
    if (!entry) {
        return NULL;
    }

    size = ((size + sizeof(allocator_entry_t) + mask) & ~mask) - sizeof(allocator_entry_t);

    entry->used = 1;
    entry->size = size;
    entry->prev = tail;
    entry->next = NULL;

    if (tail) {
        tail->next = entry;
    } else {
        head = entry;
    }

    tail = entry;

    return entry;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function allocates a block of memory of the specified size.
 *
 * PARAMETERS:
 *     size - The size of the block to allocate.
 *
 * RETURN VALUE:
 *     A pointer to the allocated block, or NULL if there is was no free entry and requesting
 *     a new page failed.
 *-----------------------------------------------------------------------------------------------*/
void *malloc(size_t size) {
    size = (size + 15) & ~0x0F;
    allocator_entry_t *entry = find_free(size);

    if (entry) {
        split_entry(entry, size);
        return entry + 1;
    }

    return NULL;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function allocates a block of memory for an array of `num` elements, zeroing it by
 *     afterwards.
 *
 * PARAMETERS:
 *     num - How many elements the array has.
 *     size - The size of each element.
 *
 * RETURN VALUE:
 *     A pointer to the allocated block, or NULL if there is was no free entry and requesting
 *     a new page failed.
 *-----------------------------------------------------------------------------------------------*/
void *calloc(size_t num, size_t size) {
    size *= num;
    void *base = malloc(size);

    if (base) {
        memset(base, 0, size);
    }

    return base;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function frees a block of memory.
 *
 * PARAMETERS:
 *     ptr - The base address of the block to free.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void free(void *ptr) {
    allocator_entry_t *entry = (allocator_entry_t *)ptr - 1;
    entry->used = 0;
    merge_forward(entry);
    merge_backward(entry);
}
