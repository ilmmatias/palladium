/* SPDX-FileCopyrightText: (C) 2023-2026 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <crt_impl/os.h>
#include <stdckdint.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef struct allocator_entry_t {
    bool used;
    size_t size;
    struct allocator_entry_t *prev, *next;
} allocator_entry_t;

static allocator_entry_t *head = NULL, *tail = NULL;

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function checks whether two allocator entries describe adjacent regions.
 *
 * PARAMETERS:
 *     left - Entry expected immediately before right.
 *     right - Entry expected immediately after left.
 *
 * RETURN VALUE:
 *     true if the entries are adjacent, false otherwise.
 *-----------------------------------------------------------------------------------------------*/
static bool is_contiguous(allocator_entry_t *left, allocator_entry_t *right) {
    uintptr_t base = (uintptr_t)(left + 1);
    return left->size <= UINTPTR_MAX - base && base + left->size == (uintptr_t)right;
}

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
    if (entry->size < size || entry->size - size <= sizeof(allocator_entry_t)) {
        return;
    }

    allocator_entry_t *new_entry = (allocator_entry_t *)((uint8_t *)(entry + 1) + size);

    new_entry->used = false;
    new_entry->size = entry->size - size - sizeof(allocator_entry_t);
    new_entry->prev = entry;
    new_entry->next = entry->next;

    if (new_entry->next) {
        new_entry->next->prev = new_entry;
    } else {
        tail = new_entry;
    }

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
    while (base->next && is_contiguous(base, base->next) && !base->next->used) {
        allocator_entry_t *next = base->next;
        size_t extra_size;
        size_t merged_size;
        if (ckd_add(&extra_size, sizeof(allocator_entry_t), next->size) ||
            ckd_add(&merged_size, base->size, extra_size)) {
            break;
        }

        base->size = merged_size;
        base->next = next->next;

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
    while (base->prev && is_contiguous(base->prev, base) && !base->prev->used) {
        allocator_entry_t *prev = base->prev;
        size_t extra_size;
        size_t merged_size;
        if (ckd_add(&extra_size, sizeof(allocator_entry_t), base->size) ||
            ckd_add(&merged_size, prev->size, extra_size)) {
            break;
        }

        prev->size = merged_size;
        prev->next = base->next;

        if (base->next) {
            base->next->prev = prev;
        }

        base = prev;
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
            entry->used = true;
            return entry;
        }

        entry = entry->next;
    }

    size_t full_size;
    if (ckd_add(&full_size, size, sizeof(allocator_entry_t)) ||
        ckd_add(&full_size, full_size, mask)) {
        return NULL;
    }

    size_t allocation_size = full_size & ~mask;
    entry = __allocate_pages(allocation_size >> __PAGE_SHIFT);
    if (!entry) {
        return NULL;
    }

    entry->used = true;
    entry->size = allocation_size - sizeof(allocator_entry_t);
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
    size_t aligned_size;
    if (ckd_add(&aligned_size, size, 15)) {
        return NULL;
    }

    aligned_size &= ~((size_t)0x0F);
    allocator_entry_t *entry = find_free(aligned_size);

    if (entry) {
        split_entry(entry, aligned_size);
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
 *     nmemb - How many elements the array has.
 *     size - The size of each element.
 *
 * RETURN VALUE:
 *     A pointer to the allocated block, or NULL if there is was no free entry and requesting
 *     a new page failed.
 *-----------------------------------------------------------------------------------------------*/
void *calloc(size_t nmemb, size_t size) {
    size_t full_size;
    if (ckd_mul(&full_size, nmemb, size)) {
        return NULL;
    }

    void *base = malloc(full_size);

    if (base) {
        memset(base, 0, full_size);
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
    if (!ptr) {
        return;
    }

    allocator_entry_t *entry = (allocator_entry_t *)ptr - 1;
    entry->used = false;
    merge_forward(entry);
    merge_backward(entry);
}
