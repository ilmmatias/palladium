/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#ifndef RT_H
#define RT_H

#include <stddef.h>
#include <stdint.h>

#define CONTAINING_RECORD(address, type, field) \
    ((type *)((char *)(address) - (uintptr_t)(&((type *)0)->field)))

typedef struct RtSinglyLinkedListEntry {
    struct RtSinglyLinkedListEntry *Next;
} RtSinglyLinkedListEntry;

typedef struct RtDoublyLinkedListEntry {
    struct RtDoublyLinkedListEntry *Next, *Prev;
} RtDoublyLinkedListEntry;

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

uint32_t RtGetHash(const void *Buffer, size_t Size);

void RtInitializeSinglyLinkedList(RtSinglyLinkedListEntry *Head);
void RtPushSinglyLinkedList(RtSinglyLinkedListEntry *Head, RtSinglyLinkedListEntry *Entry);
RtSinglyLinkedListEntry *RtPopSinglyLinkedList(RtSinglyLinkedListEntry *Head);

void RtInitializeDoublyLinkedList(RtDoublyLinkedListEntry *Head);
void RtPushDoublyLinkedList(RtDoublyLinkedListEntry *Head, RtDoublyLinkedListEntry *Entry);
void RtAppendDoublyLinkedList(RtDoublyLinkedListEntry *Head, RtDoublyLinkedListEntry *Entry);
RtDoublyLinkedListEntry *RtPopDoublyLinkedList(RtDoublyLinkedListEntry *Head);
RtDoublyLinkedListEntry *RtTruncateDoublyLinkedList(RtDoublyLinkedListEntry *Head);

#ifdef __cplusplus
}
#endif

#endif /* RT_H */
