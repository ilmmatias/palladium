/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#include <display.h>
#include <keyboard.h>
#include <stdio.h>
#include <string.h>

static int Selection = 0;
static char *Options[] = {
    "Test Entry 0",
    "Test Entry 1",
    "Test Entry 2",
    "Test Entry 3",
    "Test Entry 4",
    "Test Entry 5",
    "Test Entry 6",
    "Test Entry 7",
    "Test Entry 8",
    "Test Entry 9",
    "Test Entry 10",
    "Test Entry 11",
    "Test Entry 12",
    "Test Entry 13",
    "Test Entry 14",
    "Test Entry 15",
    "Test Entry 16",
    "Test Entry 17",
};

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
    BmSetCursor(2, 5 + Selection);
    BmSetColor(DISPLAY_COLOR_DEFAULT);
    BmClearLine(2, 2);
    BmPutString(Options[Selection]);

    BmSetCursor(2, 5 + NewSelection);
    BmSetColor(DISPLAY_COLOR_INVERSE);
    BmClearLine(2, 2);
    BmPutString(Options[NewSelection]);

    Selection = NewSelection;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function draws the menu decoration, and enters the main event loop.
 *
 * PARAMETERS:
 *     None.
 *
 * RETURN VALUE:
 *     Does not return.
 *-----------------------------------------------------------------------------------------------*/
[[noreturn]] void BmEnterMenu(void) {
    const char Header[] = "Boot Manager";
    BmSetCursor((DISPLAY_WIDTH - strlen(Header)) / 2, 0);
    BmSetColor(DISPLAY_COLOR_INVERSE);
    BmClearLine(0, 0);
    BmPutString(Header);

    const char SubHeader[] = "Choose an operating system to start.";
    BmSetCursor((DISPLAY_WIDTH - strlen(SubHeader)) / 2, 2);
    BmSetColor(DISPLAY_COLOR_HIGHLIGHT);
    BmClearLine(0, 0);
    BmPutString(SubHeader);

    const char Subtitle[] = "(Use the arrow keys to highlight your choice, then press ENTER.)";
    BmSetCursor((DISPLAY_WIDTH - strlen(Subtitle)) / 2, 3);
    BmSetColor(DISPLAY_COLOR_DEFAULT);
    BmClearLine(0, 0);
    BmPutString(Subtitle);

    int Count = sizeof(Options) / sizeof(*Options);
    if (Count > DISPLAY_HEIGHT - 6) {
        Count = DISPLAY_HEIGHT - 6;
    }

    for (int i = 0; i < Count; i++) {
        BmSetCursor(2, 5 + i);
        BmSetColor(Selection == i ? DISPLAY_COLOR_INVERSE : DISPLAY_COLOR_DEFAULT);
        BmClearLine(2, 2);
        BmPutString(Options[i]);
    }

    while (1) {
        int Key = BmPollKey();

        if (Key == KEY_UP) {
            MoveSelection(Selection ? Selection - 1 : Count - 1);
        } else if (Key == KEY_DOWN) {
            MoveSelection(Selection == Count - 1 ? 0 : Selection + 1);
        }
    }
}
