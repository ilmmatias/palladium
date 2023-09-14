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
 *     Name - Name attached to the object.
 *     NameSegs - Output; How many segments the name had; Can be NULL.
 *
 * RETURN VALUE:
 *     Pointer to the object if the entry was found, NULL otherwise.
 *-----------------------------------------------------------------------------------------------*/
AcpiObject *AcpiSearchObject(const char *Name) {
    if (!Name || !AcpipObjectTree) {
        return NULL;
    }

    char *Path = strdup(Name + 1);
    if (!Path) {
        return NULL;
    }

    AcpiObject *Namespace = AcpipObjectTree->Value.Objects;
    char *Token = strtok(Path, ".");

    /* `Name` should contain the full path/namespace for the item, and as such, we need to tokenize
       it to find the right leaf within the namespace. */
    while (Namespace) {
        if (!Token) {
            free(Path);
            return Namespace->Parent;
        }

        if (!memcmp(Namespace->Name, Token, 4)) {
            Token = strtok(NULL, ".");

            if (!Token) {
                return Namespace;
            }

            if (Namespace->Value.Type != ACPI_REGION && Namespace->Value.Type != ACPI_SCOPE &&
                Namespace->Value.Type != ACPI_DEVICE && Namespace->Value.Type != ACPI_PROCESSOR) {
                free(Path);
                return NULL;
            }

            Namespace = Namespace->Value.Objects;
            continue;
        }

        Namespace = Namespace->Next;
    }

    free(Path);
    return NULL;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function adds a new object to the global tree.
 *
 * PARAMETERS:
 *     Name - Name to be associated with the entry.
 *            This will be managed by us (aka free'd) in the case of a success!
 *     Value - Pointer to the data that will be copied into the object if the same has to be
 *             allocated anew.
 *
 * RETURN VALUE:
 *     Pointer to the allocated object (->Value memset'ed to zero), or NULL on failure.
 *-----------------------------------------------------------------------------------------------*/
AcpiObject *AcpipCreateObject(AcpipName *Name, AcpiValue *Value) {
    AcpiObject *Parent = Name->LinkedObject ? Name->LinkedObject : AcpipObjectTree;
    AcpiObject *Base = Parent->Value.Objects;

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
                if (Base->Value.Type != ACPI_REGION && Base->Value.Type != ACPI_SCOPE &&
                    Base->Value.Type != ACPI_DEVICE && Base->Value.Type != ACPI_PROCESSOR) {
                    return NULL;
                }

                Parent = Base;
                Base = Base->Value.Objects;
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
        free(Name);
        return AcpipObjectTree;
    }

    /* Final pass, searching for either the location to insert this object (end of the list),
       or a duplicate. */
    if (Base) {
        while (1) {
            if (!memcmp(Base->Name, Name->Start, 4)) {
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
        Parent->Value.Objects = Entry;
    }

    free(Name);
    return Entry;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function resolves a name string, getting its attached object in the tree.
 *
 * PARAMETERS:
 *     Name - Name attached to the object.
 *            This will be managed by us (aka free'd) in the case of a success!
 *
 * RETURN VALUE:
 *     Pointer to the object if the entry was found, NULL otherwise.
 *-----------------------------------------------------------------------------------------------*/
AcpiObject *AcpipResolveObject(AcpipName *Name) {
    AcpiObject *Parent = Name->LinkedObject ? Name->LinkedObject : AcpipObjectTree;
    AcpiObject *Base = Parent->Value.Objects;

    /* First pass, backtrack however many `^` we had prefixing this path. */
    while (Name->BacktrackCount > 0) {
        if (!Base) {
            return NULL;
        }

        Name->BacktrackCount--;
        Base = Base->Parent;
    }

    /* Second pass, validate that all required path segments in the way exist. */
    while (Name->SegmentCount > 1) {
        while (1) {
            if (!Base) {
                return NULL;
            }

            if (!memcmp(Base->Name, Name->Start, 4)) {
                if (Base->Value.Type != ACPI_REGION && Base->Value.Type != ACPI_SCOPE &&
                    Base->Value.Type != ACPI_DEVICE && Base->Value.Type != ACPI_PROCESSOR) {
                    return NULL;
                }

                Base = Base->Value.Objects;
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
        free(Name);
        return AcpipObjectTree;
    }

    /* Final pass, we need to search in the current entry, returning the object if we find it.
       If we can't find it, we fallback to the parent scope, up until we reach the root. */
    while (1) {
        if (!memcmp(Base->Name, Name->Start, 4)) {
            free(Name);
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
                Base = Parent->Value.Objects;
            } else {
                return NULL;
            }
        }
    }
}
