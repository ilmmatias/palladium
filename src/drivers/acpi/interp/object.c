/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#include <acpip.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

AcpiObject *AcpipObjectTree = NULL;

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function searches for an object/entry within the AML namespace tree.
 *
 * PARAMETERS:
 *     Parent - Which scope to search for the object.
 *     Name - Name attached to the object.
 *
 * RETURN VALUE:
 *     Pointer to the object if the entry was found, NULL otherwise.
 *-----------------------------------------------------------------------------------------------*/
AcpiObject *AcpiSearchObject(AcpiObject *Parent, const char *Name) {
    if (!Name || !AcpipObjectTree) {
        return NULL;
    } else if (!Parent) {
        Parent = AcpipObjectTree;
    }

    if (Parent->Value.Type != ACPI_DEVICE && Parent->Value.Type != ACPI_REGION &&
        Parent->Value.Type != ACPI_POWER && Parent->Value.Type != ACPI_PROCESSOR &&
        Parent->Value.Type != ACPI_THERMAL && Parent->Value.Type != ACPI_SCOPE) {
        return NULL;
    }

    for (AcpiObject *Object = Parent->Value.Children->Objects; Object; Object = Object->Next) {
        if (!memcmp(Object->Name, Name, 4)) {
            return Object;
        }
    }

    return NULL;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function evaluates the given object, either executing it as a method and returning its
 *     result, or copying its value.
 *
 * PARAMETERS:
 *     Object - The object to be evaluated.
 *     Result - Output; Where to store the result.
 *     ExpectedType - What type we want; we'll automatically CastToX() if this isn't ACPI_EMPTY.
 *
 * RETURN VALUE:
 *     1 on success, 0 otherwise.
 *-----------------------------------------------------------------------------------------------*/
int AcpiEvaluateObject(AcpiObject *Object, AcpiValue *Result, int ExpectedType) {
    if (!Object || !Result) {
        return 0;
    }

    AcpiValue MethodResult;
    AcpiValue *Value = &Object->Value;
    if (Value->Type == ACPI_METHOD) {
        /* It should be safe to assume that a mismatch in the argument size means something's gone
           wrong?*/
        if (Value->Method.Flags & 0x07) {
            return 0;
        } else if (!AcpiExecuteMethod(Object, 0, NULL, &MethodResult)) {
            return 0;
        }

        Value = &MethodResult;
    }

    AcpiCreateReference(Value, Result);
    if (Value != &Object->Value) {
        AcpiRemoveReference(Value, 0);
    }

    /* Equal types means we're done. */
    if (ExpectedType == ACPI_EMPTY || ExpectedType == Value->Type) {
        return 1;
    }

    switch (ExpectedType) {
        case ACPI_INTEGER: {
            uint64_t Integer;
            if (!AcpipCastToInteger(Result, &Integer, 1)) {
                return 0;
            }

            Result->Type = ACPI_INTEGER;
            Result->References = 1;
            Result->Integer = Integer;
            break;
        }

        case ACPI_STRING: {
            if (!AcpipCastToString(Result, 1, 0)) {
                return 0;
            }

            break;
        }

        case ACPI_BUFFER: {
            if (!AcpipCastToBuffer(Result)) {
                return 0;
            }

            break;
        }

        default: {
            if (Value->Type != ExpectedType) {
                AcpiRemoveReference(Result, 0);
                return 0;
            }
        }
    }

    return 1;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function adds a new object to the global tree.
 *
 * PARAMETERS:
 *     Name - Name to be associated with the entry.
 *     Value - Pointer to the data that will be copied into the object if the same has to be
 *             allocated anew.
 *
 * RETURN VALUE:
 *     Pointer to the allocated object (->Value memset'ed to zero), or NULL on failure.
 *-----------------------------------------------------------------------------------------------*/
AcpiObject *AcpipCreateObject(AcpipName *Name, AcpiValue *Value) {
    AcpiObject *Parent = Name->LinkedObject ? Name->LinkedObject : AcpipObjectTree;
    AcpiObject *Base = Parent->Value.Children->Objects;

    /* First pass, backtrack however many `^` we had prefixing this path. */
    while (Name->BacktrackCount > 0) {
        if (!Base) {
            return NULL;
        }

        Name->BacktrackCount--;
        Base = Base->Parent;
        Parent = Base->Parent;
    }

    /* Second pass, validate that all required path segments in the way exist. */
    while (Name->SegmentCount > 1) {
        while (1) {
            if (!Base) {
                return NULL;
            }

            if (!memcmp(Base->Name, Name->Start, 4)) {
                if (Base->Value.Type != ACPI_DEVICE && Base->Value.Type != ACPI_REGION &&
                    Base->Value.Type != ACPI_POWER && Base->Value.Type != ACPI_PROCESSOR &&
                    Base->Value.Type != ACPI_THERMAL && Base->Value.Type != ACPI_SCOPE) {
                    return NULL;
                }

                Parent = Base;
                Base = Base->Value.Children->Objects;
                break;
            }

            if (!Base->Next) {
                return NULL;
            }

            Base = Base->Next;
        }

        Name->SegmentCount--;
        Name->Start += 4;
    }

    if (!Name->SegmentCount) {
        return AcpipObjectTree;
    }

    /* Final pass, searching for either the location to insert this object (end of the list),
       or a duplicate. */
    if (Base) {
        while (1) {
            if (!memcmp(Base->Name, Name->Start, 4)) {
                if (Base->Value.Type != ACPI_DEVICE && Base->Value.Type != ACPI_REGION &&
                    Base->Value.Type != ACPI_POWER && Base->Value.Type != ACPI_PROCESSOR &&
                    Base->Value.Type != ACPI_THERMAL && Base->Value.Type != ACPI_SCOPE) {
                    AcpipShowDebugMessage("duplicate object, top most name %.4s\n", Base->Name);
                }

                return Base;
            }

            if (!Base->Next) {
                break;
            }

            Base = Base->Next;
        }
    }

    AcpiObject *Entry = malloc(sizeof(AcpiObject));
    if (!Entry) {
        return 0;
    }

    memcpy(Entry->Name, Name->Start, 4);
    memcpy(&Entry->Value, Value, sizeof(AcpiValue));
    Entry->Value.References = 1;
    Entry->Next = NULL;
    Entry->Parent = Parent;

    if (Base) {
        Base->Next = Entry;
    } else {
        Parent->Value.Children->Objects = Entry;
    }

    return Entry;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function resolves a name string, getting its attached object in the tree.
 *
 * PARAMETERS:
 *     Name - Name attached to the object.
 *
 * RETURN VALUE:
 *     Pointer to the object if the entry was found, NULL otherwise.
 *-----------------------------------------------------------------------------------------------*/
AcpiObject *AcpipResolveObject(AcpipName *Name) {
    /* First pass, backtrack however many `^` we had prefixing this path. */
    AcpiObject *Parent = Name->LinkedObject ? Name->LinkedObject : AcpipObjectTree;
    while (Name->BacktrackCount > 0) {
        if (!Parent) {
            return NULL;
        }

        Name->BacktrackCount--;
        Parent = Parent->Parent;
    }

    /* Second pass, validate that all required path segments in the way exist. */
    AcpiObject *Base = Parent->Value.Children->Objects;
    while (Name->SegmentCount > 1) {
        while (1) {
            if (!Base) {
                return NULL;
            }

            if (!memcmp(Base->Name, Name->Start, 4)) {
                if (Base->Value.Type != ACPI_DEVICE && Base->Value.Type != ACPI_REGION &&
                    Base->Value.Type != ACPI_POWER && Base->Value.Type != ACPI_PROCESSOR &&
                    Base->Value.Type != ACPI_THERMAL && Base->Value.Type != ACPI_SCOPE) {
                    return NULL;
                }

                Base = Base->Value.Children->Objects;
                break;
            }

            if (!Base->Next) {
                return NULL;
            }

            Base = Base->Next;
        }

        Name->SegmentCount--;
        Name->Start += 4;
    }

    if (!Name->SegmentCount) {
        return AcpipObjectTree;
    }

    /* Final pass, we need to search in the current entry, returning the object if we find it.
       If we can't find it, we fallback to the parent scope, up until we reach the root. */
    while (1) {
        if (!Base) {
            Parent = Parent->Parent;
            if (!Parent) {
                return NULL;
            }

            Base = Parent->Value.Children->Objects;
            continue;
        }

        if (!memcmp(Base->Name, Name->Start, 4)) {
            return Base->Value.Type == ACPI_ALIAS ? Base->Value.Alias : Base;
        }

        if (Base->Next) {
            Base = Base->Next;
        } else {
            /* Base->Parent should give us the object/scope that contains the current tree leaf,
               and it should be safe to assume it exists; ->Parent->Parent should give us the
               previous scope. */
            Parent = Base->Parent->Parent;
            if (Parent) {
                Base = Parent->Value.Children->Objects;
            } else {
                return NULL;
            }
        }
    }
}
