/* SPDX-FileCopyrightText: (C) 2023-2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#ifndef _RT_AVLTREE_H_
#define _RT_AVLTREE_H_

#include <stddef.h>
#include <stdint.h>

#ifndef CONTAINING_RECORD
#define CONTAINING_RECORD(address, type, field) \
    ((type *)((char *)(address) - (uintptr_t)(&((type *)0)->field)))
#endif /* CONTAINING_RECORD */

struct RtAvlNode;

typedef enum {
    RT_AVL_COMPARE_RESULT_LEFT,
    RT_AVL_COMPARE_RESULT_RIGHT,
    RT_AVL_COMPARE_RESULT_EQUAL,
} RtAvlCompareResult;

typedef RtAvlCompareResult (
    *RtAvlCompareRoutine)(struct RtAvlNode *FirstStruct, struct RtAvlNode *SecondStruct);

typedef struct RtAvlNode {
    int Height;
    int SubtreeSize;
    struct RtAvlNode *Parent;
    struct RtAvlNode *Left;
    struct RtAvlNode *Right;
} RtAvlNode;

typedef struct {
    int Size;
    RtAvlNode *Root;
    RtAvlCompareRoutine CompareRoutine;
} RtAvlTree;

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

void RtInitializeAvlTree(RtAvlTree *Tree, RtAvlCompareRoutine CompareRoutine);
bool RtInsertAvlTree(RtAvlTree *Tree, RtAvlNode *Node);
RtAvlNode *RtRemoveAvlTree(RtAvlTree *Tree, RtAvlNode *NodeToRemove);
int RtQueryHeightAvlTree(RtAvlTree *Tree);
int RtQuerySizeAvlTree(RtAvlTree *Tree);
RtAvlNode *RtLookupAvlTree(RtAvlTree *Tree, RtAvlNode *NodeToCompare);
RtAvlNode *RtLookupByIndexAvlTree(RtAvlTree *Tree, int Index);
RtAvlNode *RtEnumerateAvlTree(RtAvlTree *Tree, RtAvlNode **RestartKey);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _RT_AVLTREE_H_ */
