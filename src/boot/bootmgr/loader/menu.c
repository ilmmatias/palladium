/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#include <boot.h>
#include <display.h>
#include <font.h>
#include <keyboard.h>
#include <registry.h>
#include <stdlib.h>
#include <string.h>

static struct {
    uint32_t Type;
    char *Name;
    char *BootDevice;
    char *SystemFolder;
} Options[32];

static uint32_t Selection = 0;
static uint32_t OptionCount = 0;

extern uint16_t BiVideoWidth;
extern uint16_t BiVideoHeight;

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function moves the highlight into another item, removing it from the previous one.
 *
 * PARAMETERS:
 *     NewSelection - Item we're moving to.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
static void MoveSelection(int NewSelection) {
    /* Redrawing the whole screen/selection pane could cause tearing; But we can redraw only
       what changed. */
    BmSetCursor(32, BiFont.Height * (5 + Selection));
    BmSetColor(DISPLAY_COLOR_DEFAULT);
    BmClearLine(32, 32);
    BmPutString(Options[Selection].Name);

    BmSetCursor(32, BiFont.Height * (5 + NewSelection));
    BmSetColor(DISPLAY_COLOR_INVERSE);
    BmClearLine(32, 32);
    BmPutString(Options[NewSelection].Name);

    Selection = NewSelection;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function loads all the menu entries, panicking if the registry seems to be in an
 *     unsafe state.
 *
 * PARAMETERS:
 *     None.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void BmLoadMenuEntries(void) {
    /* Selection, Timeout, and Entries are required (and required to be of the right size); Assume
       a corrupted registry if they aren't present or are of the wrong type. */
    RegEntryHeader *TimeoutHeader = BmFindRegistryEntry(BmBootRegistry, NULL, "Timeout");
    RegEntryHeader *SelectionHeader = BmFindRegistryEntry(BmBootRegistry, NULL, "DefaultSelection");
    RegEntryHeader *EntriesHeader = BmFindRegistryEntry(BmBootRegistry, NULL, "Entries");

    if (!TimeoutHeader || !SelectionHeader || !EntriesHeader ||
        TimeoutHeader->Type != REG_ENTRY_DWORD || SelectionHeader->Type != REG_ENTRY_DWORD ||
        EntriesHeader->Type != REG_ENTRY_KEY) {
        BmPanic("An error occoured while trying to setup the boot manager environment.\n"
                "The Boot Manager Registry file seems to be corrupt or of an invalid format.\n");
    }

    for (int i = 0; OptionCount < 32; i++) {
        RegEntryHeader *Entry = BmGetRegistryEntry(BmBootRegistry, EntriesHeader, i);

        if (!Entry) {
            break;
        } else if (Entry->Type != REG_ENTRY_KEY) {
            free(Entry);
            continue;
        }

        /* Type is always required; SystemFolder is required for Palladium (0), BootDevice for
           chainloading (1). */
        RegEntryHeader *Type = BmFindRegistryEntry(BmBootRegistry, Entry, "Type");
        if (!Type) {
            continue;
        } else if (Type->Type != REG_ENTRY_DWORD) {
            free(Entry);
            continue;
        }

        Options[OptionCount].Type = *((uint8_t *)Type + Type->Length - 4);

        if (!Options[OptionCount].Type) {
            RegEntryHeader *SystemFolder =
                BmFindRegistryEntry(BmBootRegistry, Entry, "SystemFolder");

            if (!SystemFolder) {
                free(Entry);
                continue;
            } else if (SystemFolder->Type != REG_ENTRY_STRING) {
                free(SystemFolder);
                free(Entry);
                continue;
            }

            Options[OptionCount].BootDevice = NULL;
            Options[OptionCount].SystemFolder =
                (char *)(SystemFolder + 1) + strlen((char *)(SystemFolder + 1)) + 1;
        } else {
            RegEntryHeader *BootDevice = BmFindRegistryEntry(BmBootRegistry, Entry, "BootDevice");

            if (!BootDevice) {
                free(Entry);
                continue;
            } else if (BootDevice->Type != REG_ENTRY_STRING) {
                free(BootDevice);
                free(Entry);
                continue;
            }

            Options[OptionCount].BootDevice =
                (char *)(BootDevice + 1) + strlen((char *)(BootDevice + 1)) + 1;
            Options[OptionCount].SystemFolder = NULL;
        }

        Options[OptionCount++].Name = (char *)(Entry + 1);
    }

    Selection = *(uint32_t *)((uint8_t *)SelectionHeader + SelectionHeader->Length - 4);
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function loads all the menu entries, draws the menu decoration, and enters the main
 *     event loop.
 *
 * PARAMETERS:
 *     None.
 *
 * RETURN VALUE:
 *     Does not return.
 *-----------------------------------------------------------------------------------------------*/
[[noreturn]] void BmEnterMenu(void) {
    while (1) {
        BmSetColor(DISPLAY_COLOR_DEFAULT);
        BmResetDisplay();

        const char Header[] = "Boot Manager";
        BmSetCursor((BiVideoWidth - BmGetStringWidth(Header)) / 2, 0);
        BmSetColor(DISPLAY_COLOR_INVERSE);
        BmClearLine(0, 0);
        BmPutString(Header);

        const char SubHeader[] = "Choose an operating system to start.";
        BmSetCursor((BiVideoWidth - BmGetStringWidth(SubHeader)) / 2, BiFont.Height * 2);
        BmSetColor(DISPLAY_COLOR_HIGHLIGHT);
        BmClearLine(0, 0);
        BmPutString(SubHeader);

        const char Subtitle[] = "(Use the arrow keys to highlight your choice, then press ENTER.)";
        BmSetCursor((BiVideoWidth - BmGetStringWidth(Subtitle)) / 2, BiFont.Height * 3);
        BmSetColor(DISPLAY_COLOR_DEFAULT);
        BmClearLine(0, 0);
        BmPutString(Subtitle);

        uint32_t Count = OptionCount;
        if (Count > (uint32_t)(BiVideoHeight - BiFont.Height * 6)) {
            Count = BiVideoHeight - BiFont.Height * 6;
        }

        for (uint32_t i = 0; i < Count; i++) {
            if (Selection == i) {
                BmSetColor(DISPLAY_COLOR_INVERSE);
            } else {
                BmSetColor(DISPLAY_COLOR_DEFAULT);
            }

            BmSetCursor(32, BiFont.Height * (5 + i));
            BmClearLine(32, 32);
            BmPutString(Options[i].Name);
        }

        while (1) {
            int Key = BmPollKey();

            if (Key == KEY_UP) {
                MoveSelection(Selection ? Selection - 1 : Count - 1);
            } else if (Key == KEY_DOWN) {
                MoveSelection(Selection == Count - 1 ? 0 : Selection + 1);
            } else if (Key == '\n') {
                switch (Options[Selection].Type) {
                    case 0:
                        BmLoadPalladium(Options[Selection].SystemFolder);
                        break;
                }

                break;
            }
        }
    }
}
