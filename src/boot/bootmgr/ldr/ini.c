/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <ctype.h>
#include <file.h>
#include <ini.h>
#include <memory.h>
#include <string.h>

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function skips any number of whitespaces leading some element.
 *
 * PARAMETERS:
 *     Buffer - What we're parsing.
 *     Position - I/O; Our current position.
 *     Size - Size of the buffer.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
static void SkipSpaces(const char *Buffer, uint64_t *Position, uint64_t Size) {
    while (*Position < Size && isspace(Buffer[*Position])) {
        (*Position)++;
    }
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function skips a comment if it is the current element.
 *
 * PARAMETERS:
 *     Buffer - What we're parsing.
 *     Position - I/O; Our current position.
 *     Size - Size of the buffer.
 *
 * RETURN VALUE:
 *     1 if we either read a comment, or found the end of the file, 0 otherwise.
 *-----------------------------------------------------------------------------------------------*/
static int SkipComment(const char *Buffer, uint64_t *Position, uint64_t Size) {
    if (*Position >= Size) {
        return 1;
    } else if (Buffer[*Position] != ';') {
        return 0;
    }

    (*Position)++;
    while (*Position < Size && Buffer[*Position] != '\n') {
        (*Position)++;
    }

    return 1;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function reads a section/key name, or a key value.
 *
 * PARAMETERS:
 *     Buffer - What we're parsing.
 *     Position - I/O; Our current position.
 *     Size - Size of the buffer.
 *     Stop - Which character should make us stop.
 *
 * RETURN VALUE:
 *     End of what we read.
 *-----------------------------------------------------------------------------------------------*/
static const char *SkipName(const char *Buffer, uint64_t *Position, uint64_t Size, char Stop) {
    /* Leading spaces don't matter for us, neither do any trailing spaces before the stop. */
    do {
        if (isspace(Buffer[*Position])) {
            const char *PossibleEnd = &Buffer[*Position];

            while (*Position < Size && isspace(Buffer[*Position])) {
                (*Position)++;
            }

            if (*Position < Size && Buffer[*Position] == Stop) {
                return PossibleEnd;
            }
        } else {
            (*Position)++;
        }
    } while (*Position < Size && Buffer[*Position] != Stop);

    return &Buffer[*Position];
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function cleans up the value list of an array.
 *
 * PARAMETERS:
 *     ListHead - RtSList entry containing the list head.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
static void CleanupArray(RtSList *ListHead) {
    RtSList *ListHeader;
    while ((ListHeader = RtPopSList(ListHead))) {
        BmFreeBlock(CONTAINING_RECORD(ListHeader, BmIniArray, ListHeader));
    }
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function cleans up the section list after something went wrong.
 *
 * PARAMETERS:
 *     ListHead - RtSList entry containing the list head.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
static void Cleanup(RtSList *ListHead) {
    RtSList *SectionHeader;
    RtSList *ValueHeader;

    while ((SectionHeader = RtPopSList(ListHead))) {
        BmIniSection *Section = CONTAINING_RECORD(SectionHeader, BmIniSection, ListHeader);

        while ((ValueHeader = RtPopSList(&Section->ValueListHead))) {
            BmIniValue *Value = CONTAINING_RECORD(ValueHeader, BmIniValue, ListHeader);

            if (Value->Type == BM_INI_ARRAY) {
                CleanupArray(&Value->ArrayListHead);
            }

            BmFreeBlock(Value);
        }
    }
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function opens and parses the given configuration file.
 *
 * PARAMETERS:
 *     Path - Full path to the configuration file.
 *
 * RETURN VALUE:
 *     Handle to the INI data, or NULL on failure.
 *-----------------------------------------------------------------------------------------------*/
BmIniHandle *BmOpenIniFile(const char *Path) {
    BmFile *File = BmOpenFile(Path);
    if (!File) {
        return NULL;
    }

    char *Buffer = BmAllocateBlock(File->Size);
    if (!Buffer) {
        BmCloseFile(File);
        return NULL;
    }

    if (!BmReadFile(File, 0, File->Size, Buffer)) {
        BmFreeBlock(Buffer);
        BmCloseFile(File);
        return NULL;
    }

    uint64_t Size = File->Size;
    uint64_t Position = 0;
    BmCloseFile(File);

    RtSList SectionStack = {};
    uint32_t Sections = 0;

    BmIniSection *CurrentSection = BmAllocateZeroBlock(1, sizeof(BmIniSection) + 1);
    if (!CurrentSection) {
        BmFreeBlock(Buffer);
        return NULL;
    }

    CurrentSection->Name = (char *)(CurrentSection + 1);
    RtPushSList(&SectionStack, &CurrentSection->ListHeader);

    /* INI (or in our case INI-like) files need to be parsed line-by-line;
       We support the standard features (keys+sections+comments), and a few of our extensions
       (like arrays), just what's required to load up our configuration files. */
    while (Position < Size) {
        SkipSpaces(Buffer, &Position, Size);
        if (SkipComment(Buffer, &Position, Size)) {
            continue;
        }

        if (Position >= Size) {
            break;
        }

        /* This should be either a key (any character but an '['), or a section ('['). */
        if (Buffer[Position] == '[') {
            if (Position + 1 >= Size) {
                break;
            }

            const char *NameStart = &Buffer[++Position];
            const char *NameEnd = SkipName(Buffer, &Position, Size, ']');
            uintptr_t NameSize = NameEnd - NameStart;
            if (++Position >= Size) {
                break;
            }

            BmIniSection *Section = BmAllocateBlock(sizeof(BmIniSection) + NameSize + 1);
            if (!Section) {
                Cleanup(&SectionStack);
                BmFreeBlock(Buffer);
                return NULL;
            }

            Section->Name = (char *)(Section + 1);
            memcpy(Section->Name, NameStart, NameSize);
            Section->Name[NameSize] = 0;

            Section->ValueListHead.Next = NULL;
            RtPushSList(&SectionStack, &Section->ListHeader);
            CurrentSection = Section;
            Sections++;

            continue;
        }

        const char *NameStart = &Buffer[Position];
        const char *NameEnd = SkipName(Buffer, &Position, Size, '=');
        uintptr_t NameSize = NameEnd - NameStart;
        if (++Position >= Size) {
            break;
        }

        /* We have two valid types of values: arrays (prefixed by `[`) and strings (everything
           else).
           For arrays, we have one item per line. */
        SkipSpaces(Buffer, &Position, Size);
        if (Position >= Size) {
            break;
        }

        if (Buffer[Position] == '[') {
            RtSList ArrayListHead = {};

            Position++;
            while (1) {
                SkipSpaces(Buffer, &Position, Size);
                if (Position >= Size || Buffer[Position] == ']') {
                    break;
                }

                const char *ValueStart = &Buffer[Position];
                const char *ValueEnd = SkipName(Buffer, &Position, Size, '\n');
                uintptr_t ValueSize = ValueEnd - ValueStart;

                BmIniArray *Value = BmAllocateBlock(sizeof(BmIniArray) + ValueSize + 1);
                if (!Value) {
                    CleanupArray(&ArrayListHead);
                    Cleanup(&SectionStack);
                    BmFreeBlock(Buffer);
                    return NULL;
                }

                Value->Value = (char *)(Value + 1);
                memcpy(Value->Value, ValueStart, ValueSize);
                Value->Value[ValueSize] = 0;

                RtPushSList(&ArrayListHead, &Value->ListHeader);
            }

            Position++;

            BmIniValue *Value = BmAllocateBlock(sizeof(BmIniValue) + NameSize + 1);
            if (!Value) {
                CleanupArray(&ArrayListHead);
                Cleanup(&SectionStack);
                BmFreeBlock(Buffer);
                return NULL;
            }

            Value->Name = (char *)(Value + 1);
            memcpy(Value->Name, NameStart, NameSize);
            Value->Name[NameSize] = 0;

            Value->Type = BM_INI_ARRAY;
            memcpy(&Value->ArrayListHead, &ArrayListHead, sizeof(RtSList));
            RtPushSList(&CurrentSection->ValueListHead, &Value->ListHeader);

            continue;
        }

        const char *ValueStart = &Buffer[Position];
        const char *ValueEnd = SkipName(Buffer, &Position, Size, '\n');
        uintptr_t ValueSize = ValueEnd - ValueStart;
        if (Position++ >= Size) {
            break;
        }

        BmIniValue *Value = BmAllocateBlock(sizeof(BmIniValue) + NameSize + ValueSize + 2);
        if (!Value) {
            Cleanup(&SectionStack);
            BmFreeBlock(Buffer);
            return NULL;
        }

        Value->Name = (char *)(Value + 1);
        memcpy(Value->Name, NameStart, NameSize);
        Value->Name[NameSize] = 0;

        Value->StringValue = Value->Name + NameSize + 1;
        memcpy(Value->StringValue, ValueStart, ValueSize);
        Value->StringValue[ValueSize] = 0;

        Value->Type = BM_INI_STRING;
        RtPushSList(&CurrentSection->ValueListHead, &Value->ListHeader);
    }

    BmFreeBlock(Buffer);
    BmIniHandle *Handle = BmAllocateBlock(sizeof(BmIniHandle));
    if (!Handle) {
        Cleanup(&SectionStack);
        return NULL;
    }

    memcpy(&Handle->SectionListHead, &SectionStack, sizeof(RtSList));
    Handle->Sections = Sections;

    return Handle;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function closes the given INI handle, returning all its memory to the system.
 *
 * PARAMETERS:
 *     Handle - What we're freeing.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void BmCloseIniFile(BmIniHandle *Handle) {
    Cleanup(&Handle->SectionListHead);
    BmFreeBlock(Handle);
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function gets the given value, inside of the specified section.
 *
 * PARAMETERS:
 *     Handle - INI file handle.
 *     SectionName - Which section to use, or NULL for the root section.
 *     ValueName - Name of the value.
 *     Type - Type of the value.
 *
 * RETURN VALUE:
 *     Either a pointer to the BmIniValue, or NULL on failure.
 *-----------------------------------------------------------------------------------------------*/
BmIniValue *
BmGetIniValue(BmIniHandle *Handle, const char *SectionName, const char *ValueName, int Type) {
    if (!SectionName) {
        SectionName = "";
    }

    for (RtSList *SectionHeader = Handle->SectionListHead.Next; SectionHeader;
         SectionHeader = SectionHeader->Next) {
        BmIniSection *Section = CONTAINING_RECORD(SectionHeader, BmIniSection, ListHeader);
        if (strcmp(Section->Name, SectionName)) {
            continue;
        }

        for (RtSList *ValueHeader = Section->ValueListHead.Next; ValueHeader;
             ValueHeader = ValueHeader->Next) {
            BmIniValue *Value = CONTAINING_RECORD(ValueHeader, BmIniValue, ListHeader);

            if (strcmp(Value->Name, ValueName)) {
                continue;
            }

            return Value->Type == Type ? Value : NULL;
        }
    }

    return NULL;
}
