/* SPDX-FileCopyrightText: (C) 2026 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <rt/avltree.h>
#include <rt/bitmap.h>
#include <rt/hash.h>
#include <rt/list.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define TEST_BITMAP_BITS 130
#define TEST_AVL_NODES 257
#define TEST_RANDOM_CASES 4096

typedef struct {
    RtAvlNode Header;
    int Value;
    bool Inserted;
} TestAvlNode;

static int TestFailures;
static uint32_t RandomState = 0xC001D00D;

#define TEST_CHECK(Expression)                                                             \
    do {                                                                                   \
        if (!(Expression)) {                                                               \
            fprintf(stderr, "%s:%d: check failed: %s\n", __FILE__, __LINE__, #Expression); \
            TestFailures++;                                                                \
        }                                                                                  \
    } while (false)

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function provides the private memory fill dependency used by the production bitmap
 *     object, ensuring the host's libc implementation cannot replace the code under test.
 *
 * PARAMETERS:
 *     Destination - Buffer to fill.
 *     Value - Byte value to write.
 *     Size - Number of bytes to fill.
 *
 * RETURN VALUE:
 *     Destination.
 *-----------------------------------------------------------------------------------------------*/
void *PalladiumHostMemset(void *Destination, int Value, size_t Size) {
    uint8_t *Bytes = Destination;
    for (size_t Index = 0; Index < Size; Index++) {
        Bytes[Index] = Value;
    }

    return Destination;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function returns the next value in the deterministic test pseudo-random sequence.
 *
 * PARAMETERS:
 *     None.
 *
 * RETURN VALUE:
 *     The next pseudo-random value.
 *-----------------------------------------------------------------------------------------------*/
static uint32_t NextRandom(void) {
    RandomState ^= RandomState << 13;
    RandomState ^= RandomState >> 17;
    RandomState ^= RandomState << 5;
    return RandomState;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function compares two test AVL nodes.
 *
 * PARAMETERS:
 *     FirstHeader - First node in the comparison.
 *     SecondHeader - Second node in the comparison.
 *
 * RETURN VALUE:
 *     The direction from the first node to the second node, or equal for matching values.
 *-----------------------------------------------------------------------------------------------*/
static RtAvlCompareResult CompareAvlNodes(RtAvlNode *FirstHeader, RtAvlNode *SecondHeader) {
    TestAvlNode *First = CONTAINING_RECORD(FirstHeader, TestAvlNode, Header);
    TestAvlNode *Second = CONTAINING_RECORD(SecondHeader, TestAvlNode, Header);

    if (First->Value > Second->Value) {
        return RT_AVL_COMPARE_RESULT_LEFT;
    }

    if (First->Value < Second->Value) {
        return RT_AVL_COMPARE_RESULT_RIGHT;
    }

    return RT_AVL_COMPARE_RESULT_EQUAL;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function verifies the structure and cached metrics of an AVL subtree.
 *
 * PARAMETERS:
 *     Node - Root node of the subtree.
 *     Parent - Expected parent of the root node.
 *     Minimum - Exclusive lower value bound.
 *     Maximum - Exclusive upper value bound.
 *     Height - Where to return the computed subtree height.
 *
 * RETURN VALUE:
 *     Number of nodes in the subtree.
 *-----------------------------------------------------------------------------------------------*/
static int
VerifyAvlSubtree(RtAvlNode *Node, RtAvlNode *Parent, int Minimum, int Maximum, int *Height) {
    if (!Node) {
        *Height = -1;
        return 0;
    }

    TestAvlNode *Entry = CONTAINING_RECORD(Node, TestAvlNode, Header);
    int LeftHeight;
    int RightHeight;
    int LeftSize = VerifyAvlSubtree(Node->Left, Node, Minimum, Entry->Value, &LeftHeight);
    int RightSize = VerifyAvlSubtree(Node->Right, Node, Entry->Value, Maximum, &RightHeight);

    TEST_CHECK(Node->Parent == Parent);
    TEST_CHECK(Entry->Value > Minimum);
    TEST_CHECK(Entry->Value < Maximum);
    TEST_CHECK(LeftHeight - RightHeight >= -1);
    TEST_CHECK(LeftHeight - RightHeight <= 1);

    *Height = (LeftHeight > RightHeight ? LeftHeight : RightHeight) + 1;
    TEST_CHECK(Node->Height == *Height);
    TEST_CHECK(Node->SubtreeSize == LeftSize + RightSize + 1);
    return LeftSize + RightSize + 1;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function verifies all public lookup and enumeration behavior of an AVL tree.
 *
 * PARAMETERS:
 *     Tree - Tree to verify.
 *     Nodes - Complete set of test nodes.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
static void VerifyAvlTree(RtAvlTree *Tree, TestAvlNode *Nodes) {
    int Height;
    int ExpectedSize = 0;
    int ActualSize = VerifyAvlSubtree(Tree->Root, NULL, -1, TEST_AVL_NODES, &Height);

    for (int Index = 0; Index < TEST_AVL_NODES; Index++) {
        ExpectedSize += Nodes[Index].Inserted;
    }

    TEST_CHECK(ActualSize == ExpectedSize);
    TEST_CHECK(RtQuerySizeAvlTree(Tree) == ExpectedSize);
    TEST_CHECK(RtQueryHeightAvlTree(Tree) == (Tree->Root ? Height + 1 : 0));

    RtAvlNode *RestartKey = NULL;
    int PreviousValue = -1;
    int EnumerationCount = 0;
    RtAvlNode *Header;
    while ((Header = RtEnumerateAvlTree(Tree, &RestartKey))) {
        TestAvlNode *Entry = CONTAINING_RECORD(Header, TestAvlNode, Header);
        TEST_CHECK(Entry->Value > PreviousValue);
        TEST_CHECK(RtLookupByIndexAvlTree(Tree, EnumerationCount) == Header);
        PreviousValue = Entry->Value;
        EnumerationCount++;
    }

    TEST_CHECK(EnumerationCount == ExpectedSize);
    TEST_CHECK(RtLookupByIndexAvlTree(Tree, -1) == NULL);
    TEST_CHECK(RtLookupByIndexAvlTree(Tree, ExpectedSize) == NULL);
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function executes deterministic singly and doubly linked list tests.
 *
 * PARAMETERS:
 *     None.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
static void TestLists(void) {
    RtSList First = {0};
    RtSList Second = {0};
    RtSList Entries[4] = {0};

    RtPushSList(&First, &Entries[0]);
    RtPushSList(&First, &Entries[1]);
    RtPushSList(&Second, &Entries[2]);
    RtPushSList(&Second, &Entries[3]);
    RtSpliceSList(&First, &Second);
    TEST_CHECK(Second.Next == NULL);
    TEST_CHECK(RtPopSList(&First) == &Entries[3]);
    TEST_CHECK(RtPopSList(&First) == &Entries[2]);
    TEST_CHECK(RtPopSList(&First) == &Entries[1]);
    TEST_CHECK(RtPopSList(&First) == &Entries[0]);
    TEST_CHECK(RtPopSList(&First) == NULL);

    RtDList FirstHead;
    RtDList SecondHead;
    RtDList DEntries[4];
    RtInitializeDList(&FirstHead);
    RtInitializeDList(&SecondHead);
    RtPushDList(&FirstHead, &DEntries[1]);
    RtPushDList(&FirstHead, &DEntries[0]);
    RtAppendDList(&SecondHead, &DEntries[2]);
    RtAppendDList(&SecondHead, &DEntries[3]);
    RtSpliceTailDList(&FirstHead, &SecondHead);
    for (int Index = 0; Index < 4; Index++) {
        TEST_CHECK(RtPopDList(&FirstHead) == &DEntries[Index]);
    }
    TEST_CHECK(RtPopDList(&FirstHead) == &FirstHead);
    TEST_CHECK(SecondHead.Next == &SecondHead && SecondHead.Prev == &SecondHead);
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function performs the reference search used by the bitmap model tests.
 *
 * PARAMETERS:
 *     Model - Reference bitmap values.
 *     Hint - Search starting point.
 *     Count - Required run length.
 *     Value - Value to search for.
 *
 * RETURN VALUE:
 *     Start of the first matching run, or UINT64_MAX when no run exists.
 *-----------------------------------------------------------------------------------------------*/
static uint64_t FindModelBits(bool *Model, uint64_t Hint, uint64_t Count, bool Value) {
    if (Hint >= TEST_BITMAP_BITS) {
        Hint = 0;
    }

    if (Count > TEST_BITMAP_BITS) {
        return UINT64_MAX;
    }

    if (!Count) {
        return Hint;
    }

    uint64_t Starts[2] = {Hint, 0};
    uint64_t Ends[2] = {TEST_BITMAP_BITS, Hint};
    int Attempts = Hint ? 2 : 1;
    for (int Attempt = 0; Attempt < Attempts; Attempt++) {
        if (Count > Ends[Attempt] - Starts[Attempt]) {
            continue;
        }

        for (uint64_t Start = Starts[Attempt]; Start <= Ends[Attempt] - Count; Start++) {
            uint64_t Bit;
            for (Bit = 0; Bit < Count && Model[Start + Bit] == Value; Bit++) {
            }

            if (Bit == Count) {
                return Start;
            }
        }
    }

    return UINT64_MAX;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function executes boundary and randomized bitmap model tests.
 *
 * PARAMETERS:
 *     None.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
static void TestBitmap(void) {
    uint64_t Buffer[3] = {0};
    bool Model[TEST_BITMAP_BITS] = {0};
    RtBitmap Bitmap;
    RtInitializeBitmap(&Bitmap, Buffer, TEST_BITMAP_BITS);

    TEST_CHECK(RtFindClearBits(&Bitmap, 0, TEST_BITMAP_BITS) == 0);
    RtSetAllBits(&Bitmap);
    memset(Model, 1, sizeof(Model));
    RtClearBits(&Bitmap, 120, 10);
    memset(Model + 120, 0, 10);
    TEST_CHECK(RtFindClearBits(&Bitmap, 120, 10) == 120);
    TEST_CHECK(RtFindClearBits(&Bitmap, 121, 9) == 121);
    TEST_CHECK(RtFindClearBits(&Bitmap, 121, 10) == UINT64_MAX);

    for (int Case = 0; Case < TEST_RANDOM_CASES; Case++) {
        uint64_t Start = NextRandom() % TEST_BITMAP_BITS;
        uint64_t Count = NextRandom() % (TEST_BITMAP_BITS - Start + 1);
        bool Value = (NextRandom() & 1) != 0;

        if (Value) {
            RtSetBits(&Bitmap, Start, Count);
        } else {
            RtClearBits(&Bitmap, Start, Count);
        }

        for (uint64_t Bit = 0; Bit < Count; Bit++) {
            Model[Start + Bit] = Value;
        }

        uint64_t Hint = NextRandom() % (TEST_BITMAP_BITS + 8);
        uint64_t Required = NextRandom() % (TEST_BITMAP_BITS + 2);
        TEST_CHECK(
            RtFindClearBits(&Bitmap, Hint, Required) ==
            FindModelBits(Model, Hint, Required, false));
        TEST_CHECK(
            RtFindSetBits(&Bitmap, Hint, Required) == FindModelBits(Model, Hint, Required, true));
    }
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function executes deterministic and randomized AVL model tests.
 *
 * PARAMETERS:
 *     None.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
static void TestAvl(void) {
    TestAvlNode Nodes[TEST_AVL_NODES] = {0};
    RtAvlTree Tree;
    RtInitializeAvlTree(&Tree, CompareAvlNodes);

    for (int Index = 0; Index < TEST_AVL_NODES; Index++) {
        Nodes[Index].Value = Index;
    }

    for (int Case = 0; Case < TEST_RANDOM_CASES; Case++) {
        int Index = NextRandom() % TEST_AVL_NODES;
        if (Nodes[Index].Inserted) {
            TEST_CHECK(RtRemoveAvlTree(&Tree, &Nodes[Index].Header) == &Nodes[Index].Header);
            Nodes[Index].Inserted = false;
        } else {
            TEST_CHECK(RtInsertAvlTree(&Tree, &Nodes[Index].Header));
            Nodes[Index].Inserted = true;
        }

        VerifyAvlTree(&Tree, Nodes);
    }

    for (int Index = 0; Index < TEST_AVL_NODES; Index++) {
        if (Nodes[Index].Inserted) {
            TEST_CHECK(RtRemoveAvlTree(&Tree, &Nodes[Index].Header) == &Nodes[Index].Header);
            Nodes[Index].Inserted = false;
        }
    }

    VerifyAvlTree(&Tree, Nodes);
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function executes known-vector and unaligned-input hash tests.
 *
 * PARAMETERS:
 *     None.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
static void TestHash(void) {
    static const uint8_t UnalignedData[] = {
        0xFF,
        '1',
        '2',
        '3',
        '4',
        '5',
        '6',
        '7',
        '8',
        '9',
        '0',
        '1',
        '2',
        '3',
        '4',
        '5',
        '6',
    };

    TEST_CHECK(RtGetHash("", 0) == 0x02CC5D05);
    TEST_CHECK(RtGetHash("a", 1) == 0x550D7456);
    TEST_CHECK(RtGetHash("abc", 3) == 0x32D153FF);
    TEST_CHECK(RtGetHash(UnalignedData + 1, 16) == 0x03BF5152);
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function runs all portable RT host tests.
 *
 * PARAMETERS:
 *     None.
 *
 * RETURN VALUE:
 *     Zero when every test passes, nonzero otherwise.
 *-----------------------------------------------------------------------------------------------*/
int main(void) {
    TestLists();
    TestBitmap();
    TestAvl();
    TestHash();

    if (TestFailures) {
        fprintf(stderr, "portable RT tests failed: %d\n", TestFailures);
        return 1;
    }

    puts("portable RT tests passed");
    return 0;
}
