/* SPDX-FileCopyrightText: (C) 2023-2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <rt/avltree.h>

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function gets the effective height of a node in the tree.
 *
 * PARAMETERS:
 *     Node - Which node to get the height of.
 *
 * RETURN VALUE:
 *     Either the height of the node, or -1 if the node doesn't exist.
 *-----------------------------------------------------------------------------------------------*/
static int GetHeight(RtAvlNode* Node) {
    return Node ? Node->Height : -1;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function gets the size of a node in the tree.
 *
 * PARAMETERS:
 *     Node - Which node to get the size of.
 *
 * RETURN VALUE:
 *     Either the size of the node (and its childs), or 0 if the node doesn't exist.
 *-----------------------------------------------------------------------------------------------*/
static int GetSubtreeSize(RtAvlNode* Node) {
    return Node ? Node->SubtreeSize : 0;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function calculates the balance value for a node in the tree.
 *
 * PARAMETERS:
 *     Node - Which node to get the balance of.
 *
 * RETURN VALUE:
 *     Values below 1 means the node is right-heavy, values above 1 means the node is left-heavy,
 *     0 means the node is balanced.
 *-----------------------------------------------------------------------------------------------*/
static int GetBalance(RtAvlNode* Node) {
    return GetHeight(Node->Left) - GetHeight(Node->Right);
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function recalculates the height of a node using the height of its childs.
 *
 * PARAMETERS:
 *     Node - Which node we need to recalculate the height of.
 *
 * RETURN VALUE:
 *     New height to be assigned into Node->Height.
 *-----------------------------------------------------------------------------------------------*/
static int RecalculateHeight(RtAvlNode* Node) {
    int LeftHeight = GetHeight(Node->Left);
    int RightHeight = GetHeight(Node->Right);
    return (LeftHeight < RightHeight ? RightHeight : LeftHeight) + 1;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function recalculates the size of a node using the size of its childs.
 *
 * PARAMETERS:
 *     Node - Which node we need to recalculate the size of.
 *
 * RETURN VALUE:
 *     New size to be assigned into Node->SubtreeSize.
 *-----------------------------------------------------------------------------------------------*/
static int RecalculateSubtreeSize(RtAvlNode* Node) {
    return GetSubtreeSize(Node->Left) + GetSubtreeSize(Node->Right) + 1;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function performs a left rotation around the given node.
 *
 * PARAMETERS:
 *     Node - Target node to perform the rotation.
 *
 * RETURN VALUE:
 *     New node root after the rotation.
 *-----------------------------------------------------------------------------------------------*/
static RtAvlNode* RotateLeft(RtAvlNode* Node) {
    RtAvlNode* NewRoot = Node->Right;
    RtAvlNode* Center = NewRoot->Left;
    RtAvlNode* OldParent = Node->Parent;

    // Perform the actual rotation.
    NewRoot->Left = Node;
    Node->Right = Center;

    // Update the parent pointers.
    NewRoot->Parent = OldParent;
    Node->Parent = NewRoot;
    if (Center) {
        Center->Parent = Node;
    }

    // Update heights and subtree sizes. Order is important, don't swap it around.
    Node->Height = RecalculateHeight(Node);
    Node->SubtreeSize = RecalculateSubtreeSize(Node);
    NewRoot->Height = RecalculateHeight(NewRoot);
    NewRoot->SubtreeSize = RecalculateSubtreeSize(NewRoot);

    return NewRoot;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function performs a right rotation around the given node.
 *
 * PARAMETERS:
 *     Node - Target node to perform the rotation.
 *
 * RETURN VALUE:
 *     New node root after the rotation.
 *-----------------------------------------------------------------------------------------------*/
static RtAvlNode* RotateRight(RtAvlNode* Node) {
    RtAvlNode* NewRoot = Node->Left;
    RtAvlNode* Center = NewRoot->Right;
    RtAvlNode* OldParent = Node->Parent;

    // Perform the actual rotation.
    NewRoot->Right = Node;
    Node->Left = Center;

    // Update the parent pointers.
    NewRoot->Parent = OldParent;
    Node->Parent = NewRoot;
    if (Center) {
        Center->Parent = Node;
    }

    // Update heights and subtree sizes. Order is important, don't swap it around.
    Node->Height = RecalculateHeight(Node);
    Node->SubtreeSize = RecalculateSubtreeSize(Node);
    NewRoot->Height = RecalculateHeight(NewRoot);
    NewRoot->SubtreeSize = RecalculateSubtreeSize(NewRoot);

    return NewRoot;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function searches the given AVL tree for either a node matching the value of the given
 *     node, or the parent and the expected insertion point of the value into that parent.
 *
 * PARAMETERS:
 *     Tree - AVL tree to perform the search on.
 *     NodeToCompare - Template node containing the value we want to find.
 *     SearchResult - Where to store either the found node or its parent.
 *
 * RETURN VALUE:
 *     Insertion point in case we didn't find the node (or EQUAL if we did find it).
 *-----------------------------------------------------------------------------------------------*/
static RtAvlCompareResult
SearchNodeOrParent(RtAvlTree* Tree, RtAvlNode* NodeToCompare, RtAvlNode** SearchResult) {
    RtAvlCompareResult CompareResult = RT_AVL_COMPARE_RESULT_LEFT;
    RtAvlNode* ParentNode = NULL;

    *SearchResult = Tree->Root;

    while (true) {
        if (!*SearchResult) {
            *SearchResult = ParentNode;
            return CompareResult;
        }

        CompareResult = Tree->CompareRoutine(*SearchResult, NodeToCompare);
        ParentNode = *SearchResult;

        if (CompareResult == RT_AVL_COMPARE_RESULT_LEFT) {
            *SearchResult = (*SearchResult)->Left;
        } else if (CompareResult == RT_AVL_COMPARE_RESULT_RIGHT) {
            *SearchResult = (*SearchResult)->Right;
        } else {
            return CompareResult;
        }
    }
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function fixes the balance of the given AVL tree after an insert/remove operation.
 *
 * PARAMETERS:
 *     Tree - AVL tree to perform the rebalance on.
 *     StartNode - Which node to start the rebalance at.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
static void RebalanceTree(RtAvlTree* Tree, RtAvlNode* StartNode) {
    RtAvlNode* CurrentNode = StartNode;

    while (CurrentNode) {
        // Update metrics for the current node.
        CurrentNode->Height = RecalculateHeight(CurrentNode);
        CurrentNode->SubtreeSize = RecalculateSubtreeSize(CurrentNode);

        // Determine if a rotation is needed.
        int Balance = GetBalance(CurrentNode);
        RtAvlNode* ParentNode = CurrentNode->Parent;
        RtAvlNode* NewRoot = NULL;
        if (Balance > 1 && GetBalance(CurrentNode->Left) >= 0) {
            NewRoot = RotateRight(CurrentNode);
        } else if (Balance < -1 && GetBalance(CurrentNode->Right) <= 0) {
            NewRoot = RotateLeft(CurrentNode);
        } else if (Balance > 1 && GetBalance(CurrentNode->Left) < 0) {
            CurrentNode->Left = RotateLeft(CurrentNode->Left);
            NewRoot = RotateRight(CurrentNode);
        } else if (Balance < -1 && GetBalance(CurrentNode->Right) > 0) {
            CurrentNode->Right = RotateRight(CurrentNode->Right);
            NewRoot = RotateLeft(CurrentNode);
        }

        // If a rotation did happen, the new root of the subtree needs to be
        // linked to the original parent.
        if (NewRoot) {
            if (!ParentNode) {
                Tree->Root = NewRoot;
            } else if (ParentNode->Left == CurrentNode) {
                ParentNode->Left = NewRoot;
            } else {
                ParentNode->Right = NewRoot;
            }
        }

        CurrentNode = ParentNode;
    }
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function finds the smallest (left-most) value inside a given subtree/node.
 *
 * PARAMETERS:
 *     CurrentNode - Starting point to find the value.
 *
 * RETURN VALUE:
 *     Smallest value in the node, or NULL if the node was NULL.
 *-----------------------------------------------------------------------------------------------*/
static RtAvlNode* GetMinimumNode(RtAvlNode* CurrentNode) {
    if (!CurrentNode) {
        return NULL;
    }

    while (CurrentNode->Left) {
        CurrentNode = CurrentNode->Left;
    }

    return CurrentNode;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function finds the next (in-order) value that comes after the given node.
 *
 * PARAMETERS:
 *     Node - Starting point to find the value.
 *
 * RETURN VALUE:
 *     Successor of the current node, or NULL if there is none (or the node was NULL).
 *-----------------------------------------------------------------------------------------------*/
static RtAvlNode* GetInOrderSuccessor(RtAvlNode* Node) {
    return Node ? GetMinimumNode(Node->Right) : NULL;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function initializes an AVL tree.
 *
 * PARAMETERS:
 *     Tree - Which tree to initialize.
 *     CompareRoutine - Function to be used when doing a lookup or insertion into the tree.
 *                      It's always called with the `FirstStruct` value containing what can be
 *                      considered the "parent", and the `SecondStruct` value containing what we're
 *                      searching. Return LEFT/RIGHT/EQUAL according to that (first<second=right,
 *                      first>second=left, first=second=equal).
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void RtInitializeAvlTree(RtAvlTree* Tree, RtAvlCompareRoutine CompareRoutine) {
    Tree->Size = 0;
    Tree->Root = NULL;
    Tree->CompareRoutine = CompareRoutine;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function inserts a new node into a previously initialized tree.
 *
 * PARAMETERS:
 *     Tree - Which tree to insert the node.
 *     Node - Header of the node that you want to insert.
 *
 * RETURN VALUE:
 *     false if the node already existed in the tree, true otherwise.
 *-----------------------------------------------------------------------------------------------*/
bool RtInsertAvlTree(RtAvlTree* Tree, RtAvlNode* Node) {
    RtAvlNode* ParentNode;
    RtAvlCompareResult CompareResult = SearchNodeOrParent(Tree, Node, &ParentNode);
    if (CompareResult == RT_AVL_COMPARE_RESULT_EQUAL && ParentNode) {
        return false;
    }

    Node->Height = 0;
    Node->SubtreeSize = 1;
    Node->Parent = ParentNode;
    Node->Left = NULL;
    Node->Right = NULL;

    if (!ParentNode) {
        Tree->Root = Node;
    } else if (CompareResult == RT_AVL_COMPARE_RESULT_LEFT) {
        ParentNode->Left = Node;
    } else if (CompareResult == RT_AVL_COMPARE_RESULT_RIGHT) {
        ParentNode->Right = Node;
    }

    RebalanceTree(Tree, ParentNode);
    Tree->Size++;
    return true;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function removes an existing node from the tree.
 *
 * PARAMETERS:
 *     Tree - Which tree to remove the node from.
 *     Node - A "template" that can be passed into RtLookupAvlTree to find the node that needs to
 *            be removed.
 *
 * RETURN VALUE:
 *     Either the removed node, or NULL if we couldn't find it.
 *-----------------------------------------------------------------------------------------------*/
RtAvlNode* RtRemoveAvlTree(RtAvlTree* Tree, RtAvlNode* NodeToRemove) {
    NodeToRemove = RtLookupAvlTree(Tree, NodeToRemove);
    if (!NodeToRemove) {
        return NULL;
    }

    RtAvlNode* NodeToReplace;
    if (!NodeToRemove->Left || !NodeToRemove->Right) {
        NodeToReplace = NodeToRemove;
    } else {
        NodeToReplace = GetInOrderSuccessor(NodeToRemove);
    }

    RtAvlNode* ChildNode;
    if (NodeToReplace->Left) {
        ChildNode = NodeToReplace->Left;
    } else {
        ChildNode = NodeToReplace->Right;
    }

    // Splice out NodeToReplace by linking its child to its parent.
    RtAvlNode* RebalanceNode = NodeToReplace->Parent;
    if (ChildNode) {
        ChildNode->Parent = RebalanceNode;
    }

    if (!RebalanceNode) {
        Tree->Root = ChildNode;
    } else if (NodeToReplace == RebalanceNode->Left) {
        RebalanceNode->Left = ChildNode;
    } else {
        RebalanceNode->Right = ChildNode;
    }

    if (NodeToReplace != NodeToRemove) {
        // As we don't have any idea about the struct we're contained within,
        // we cannot simple execute a memcpy, and instead, we need to manually
        // swap the pointers around to reach the desired effect.
        NodeToReplace->Height = NodeToRemove->Height;
        NodeToReplace->SubtreeSize = NodeToRemove->SubtreeSize;
        NodeToReplace->Parent = NodeToRemove->Parent;
        NodeToReplace->Left = NodeToRemove->Left;
        NodeToReplace->Right = NodeToRemove->Right;

        if (NodeToRemove->Left) {
            NodeToRemove->Left->Parent = NodeToReplace;
        }

        if (NodeToRemove->Right) {
            NodeToRemove->Right->Parent = NodeToReplace;
        }

        if (!NodeToRemove->Parent) {
            Tree->Root = NodeToReplace;
        } else if (NodeToRemove->Parent->Left == NodeToRemove) {
            NodeToRemove->Parent->Left = NodeToReplace;
        } else {
            NodeToRemove->Parent->Right = NodeToReplace;
        }
    }

    RebalanceTree(Tree, RebalanceNode);
    Tree->Size--;
    return NodeToRemove;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function returns the current height of the tree.
 *
 * PARAMETERS:
 *     Tree - Which tree to query.
 *
 * RETURN VALUE:
 *     Either 0 if the tree is empty, or its height.
 *-----------------------------------------------------------------------------------------------*/
int RtQueryHeightAvlTree(RtAvlTree* Tree) {
    return Tree->Root ? Tree->Root->Height + 1 : 0;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function returns the current size of the tree.
 *
 * PARAMETERS:
 *     Tree - Which tree to query.
 *
 * RETURN VALUE:
 *     Either 0 if the tree is empty, or its size.
 *-----------------------------------------------------------------------------------------------*/
int RtQuerySizeAvlTree(RtAvlTree* Tree) {
    return Tree->Size;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function attempts to find a matching node inside the given tree.
 *
 * PARAMETERS:
 *     Tree - Which tree to query.
 *     Node - A "template" that can be compared against to find the target node.
 *
 * RETURN VALUE:
 *     Either the node if it was found, or NULL.
 *-----------------------------------------------------------------------------------------------*/
RtAvlNode* RtLookupAvlTree(RtAvlTree* Tree, RtAvlNode* NodeToCompare) {
    RtAvlNode* SearchResult;
    RtAvlCompareResult CompareResult;

    CompareResult = SearchNodeOrParent(Tree, NodeToCompare, &SearchResult);

    if (CompareResult == RT_AVL_COMPARE_RESULT_EQUAL) {
        return SearchResult;
    } else {
        return NULL;
    }
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function attempts to find a node inside the given tree using its in-order index.
 *
 * PARAMETERS:
 *     Tree - Which tree to query.
 *     Index - In-order index of the node inside the tree.
 *
 * RETURN VALUE:
 *     Either the node if it was found, or NULL.
 *-----------------------------------------------------------------------------------------------*/
RtAvlNode* RtLookupByIndexAvlTree(RtAvlTree* Tree, int Index) {
    RtAvlNode* CurrentNode;
    int LeftSize;

    if (Index < 0 || Index >= Tree->Size) {
        return NULL;
    }

    CurrentNode = Tree->Root;

    while (CurrentNode) {
        LeftSize = GetSubtreeSize(CurrentNode->Left);

        if (Index < LeftSize) {
            // Target is in the left subtree
            CurrentNode = CurrentNode->Left;
        } else if (Index > LeftSize) {
            // Target is in the right subtree
            Index -= LeftSize + 1;
            CurrentNode = CurrentNode->Right;
        } else {
            // Index == LeftSize, so the current node is the target
            return CurrentNode;
        }
    }

    return NULL;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function returns the next in-order node inside the given tree. Call it repeatedly to
 *     enumerate the whole tree.
 *
 * PARAMETERS:
 *     Tree - Which tree to query.
 *     RestartKey - Helper pointer used to enumerate the tree; Initialize it's contents with NULL
 *                  before the first call.
 *
 * RETURN VALUE:
 *     Either the next in-order node, or NULL if we're finished.
 *-----------------------------------------------------------------------------------------------*/
RtAvlNode* RtEnumerateAvlTree(RtAvlTree* Tree, RtAvlNode** RestartKey) {
    RtAvlNode* CurrentNode;
    RtAvlNode* ParentNode;

    if (!*RestartKey) {
        CurrentNode = GetMinimumNode(Tree->Root);
    } else if ((*RestartKey)->Right) {
        // If there's a right subtree, the successor is the left-most node in it.
        CurrentNode = GetMinimumNode((*RestartKey)->Right);
    } else {
        // Otherwise, go up the tree until we find a parent from which
        // we descended from the left.
        CurrentNode = *RestartKey;
        ParentNode = CurrentNode->Parent;

        while (ParentNode && CurrentNode == ParentNode->Right) {
            CurrentNode = ParentNode;
            ParentNode = ParentNode->Parent;
        }

        CurrentNode = ParentNode;
    }

    *RestartKey = CurrentNode;
    return *RestartKey;
}
