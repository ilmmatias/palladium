/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#include <display.h>
#include <file.h>
#include <ini.h>
#include <loader.h>
#include <memory.h>
#include <stdlib.h>
#include <string.h>

static BmIniHandle *Handle = NULL;

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function loads the bootmgr default config file, and validates that it has at least one
 *     boot entry.
 *
 * PARAMETERS:
 *     None.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void BiLoadConfig(void) {
    Handle = BmOpenIniFile("boot()/Boot/bootmgr.ini");
    if (!Handle) {
        BmPrint("Could not open the boot manager configuration file.\n"
                "You might need to repair your installation.\n");
        while (1)
            ;
    }

    if (!Handle->Sections) {
        BmPrint("The boot manager configuration file does not contain any boot entries.\n"
                "You might need to repair your installation.\n");
        while (1)
            ;
    }
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function loads up the selected entry, transferring execution to the kernel/whatever is
 *     loaded.
 *
 * PARAMETERS:
 *     Entry - Pointer to the entry data.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
[[noreturn]] void BiLoadEntry(BmMenuEntry *Entry) {
    BmResetDisplay();
    BmSetCursor(0, 0);
    BiCheckCompatibility(Entry->Type);

    switch (Entry->Type) {
        case BM_ENTRY_PALLADIUM:
            BiLoadPalladium(Entry);

        case BM_ENTRY_CHAINLOAD:
            BiLoadChainload(Entry);

        /* This really shouldn't happen. */
        default:
            BmPrint("Boot manager entry type is unimplemented.\n");
            break;
    }

    while (1)
        ;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function gets the default timeout for the menu.
 *
 * PARAMETERS:
 *     None.
 *
 * RETURN VALUE:
 *     Default timeout in seconds.
 *-----------------------------------------------------------------------------------------------*/
int BmGetDefaultTimeout(void) {
    BmIniValue *Value = BmGetIniValue(Handle, NULL, "Timeout", BM_INI_STRING);
    if (!Value) {
        return 5;
    }

    return strtol(Value->StringValue, NULL, 10);
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function gets the index of the default selection. Use this together with BiGetMenuEntry
 *     to get the default selection entry.
 *
 * PARAMETERS:
 *     None.
 *
 * RETURN VALUE:
 *     Current/default index.
 *-----------------------------------------------------------------------------------------------*/
int BmGetDefaultSelectionIndex(void) {
    BmIniValue *Value = BmGetIniValue(Handle, NULL, "DefaultSelection", BM_INI_STRING);
    if (!Value) {
        return 0;
    }

    /* The root section should always be "index -1" so to speak (as it is extra/not counted in the
       sections). */
    int Index = Handle->Sections - 1;
    RtSList *ListHeader = Handle->SectionListHead.Next;
    while (Index >= 0) {
        if (!ListHeader) {
            break;
        } else if (!strcmp(
                       CONTAINING_RECORD(ListHeader, BmIniSection, ListHeader)->Name,
                       Value->StringValue)) {
            return Index;
        }

        Index--;
        ListHeader = ListHeader->Next;
    }

    return 0;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function returns how many entries we have (value stored in our Handle->Sections).
 *
 * PARAMETERS:
 *     None.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
int BmGetMenuEntryCount(void) {
    return Handle->Sections;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function loads up the Nth menu entry. This includes preloading the used icon.
 *
 * PARAMETERS:
 *     Index - Index of the entry.
 *     Entry - Output; Where to store the entry data.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void BmGetMenuEntry(int Index, BmMenuEntry *Entry) {
    /* The root section should always be "index -1" so to speak (as it is extra/not counted in the
       sections). */
    int CurrentIndex = Handle->Sections - 1;
    RtSList *ListHeader = Handle->SectionListHead.Next;
    while (CurrentIndex >= 0) {
        if (!ListHeader || Index > CurrentIndex) {
            BmPrint("The boot manager environment seems to be corrupted.\n"
                    "You might need to repair your installation.\n");
            while (1)
                ;
        } else if (Index == CurrentIndex) {
            break;
        }

        CurrentIndex--;
        ListHeader = ListHeader->Next;
    }

    const char *Section = CONTAINING_RECORD(ListHeader, BmIniSection, ListHeader)->Name;
    BmIniValue *Type = BmGetIniValue(Handle, Section, "Type", BM_INI_STRING);
    if (!Type) {
        BmPrint(
            "The [%s] boot manager entry does not contain a `Type` field.\n"
            "You might need to repair your installation.\n",
            Section);
        while (1)
            ;
    }

    /* We'll be using default values if those don't exist (but they should). */
    BmIniValue *Text = BmGetIniValue(Handle, Section, "Text", BM_INI_STRING);
    BmIniValue *Icon = BmGetIniValue(Handle, Section, "Icon", BM_INI_STRING);

    if (!strcmp(Type->StringValue, "palladium")) {
        /* It makes no sense for us to have no root folder. */
        BmIniValue *SystemFolder = BmGetIniValue(Handle, Section, "SystemFolder", BM_INI_STRING);
        BmIniValue *Drivers = BmGetIniValue(Handle, Section, "Drivers", BM_INI_ARRAY);
        if (!SystemFolder) {
            BmPrint(
                "The [%s] boot manager entry does not contain a `SystemFolder` field.\n"
                "You might need to repair your installation.\n",
                Section);
            while (1)
                ;
        }

        Entry->Type = BM_ENTRY_PALLADIUM;
        Entry->Palladium.SystemFolder = SystemFolder->StringValue;
        Entry->Palladium.DriverListHead = Drivers ? Drivers->ArrayListHead.Next : NULL;
    } else if (!strcmp(Type->StringValue, "chainload")) {
        BmIniValue *Path = BmGetIniValue(Handle, Section, "Path", BM_INI_STRING);
        if (!Path) {
            BmPrint(
                "The [%s] boot manager entry does not contain a `Path` field.\n"
                "You might need to repair your installation.\n",
                Section);
            while (1)
                ;
        }

        Entry->Type = BM_ENTRY_CHAINLOAD;
        Entry->Chainload.Path = Path->StringValue;
    } else {
        BmPrint(
            "The [%s] boot manager entry does not contain a valid `Type` field.\n"
            "You might need to repair your installation.\n",
            Section);
        while (1)
            ;
    }

    Entry->Text = Text ? Text->StringValue : "No name";

    /* Load up the icon from the disk; We shouldn't access the disk everytime the user presses a
       key. */
    const char *IconFilename = Icon ? Icon->StringValue : "boot()/Boot/os.bmp";
    BmFile *IconHandle = BmOpenFile(IconFilename);
    if (!IconHandle) {
        BmPrint(
            "The icon file `%s` specified in the [%s] boot manager entry does not exist.\n"
            "You might need to repair your installation.\n",
            IconFilename,
            Section);
        while (1)
            ;
    }

    Entry->Icon = BmAllocateBlock(IconHandle->Size);
    if (!Entry->Icon) {
        BmPrint(
            "Could not allocate enough memory for loading the icon file `%s`.\n"
            "Your system might not have enough usable memory.\n",
            IconFilename);
        while (1)
            ;
    } else if (!BmReadFile(IconHandle, 0, IconHandle->Size, Entry->Icon)) {
        BmPrint(
            "Could not read the icon file `%s`.\n"
            "You might need to repair your installation.\n",
            IconFilename);
        while (1)
            ;
    }

    BmCloseFile(IconHandle);
}
