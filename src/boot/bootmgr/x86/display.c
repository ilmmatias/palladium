/* SPDX-FileCopyrightText: (C) 2023 yuuma03
 * SPDX-License-Identifier: BSD-3-Clause */

#include <boot.h>
#include <string.h>

#define VIDEO_MEMORY 0xB8000

#define SCREEN_WIDTH 80
#define SCREEN_HEIGHT 25
#define SCREEN_SIZE (SCREEN_WIDTH * SCREEN_HEIGHT)
#define SCREEN_ATTRIBUTE 0x07

#define TAB_SIZE 4

static uint16_t *VideoMemory = (uint16_t *)VIDEO_MEMORY;
static uint16_t CursorX = 0;
static uint16_t CursorY = 0;

static void ScrollUp(void) {
    memcpy(VideoMemory, VideoMemory + SCREEN_WIDTH, (SCREEN_SIZE - SCREEN_WIDTH) * 2);
    memset(VideoMemory + SCREEN_SIZE - SCREEN_WIDTH, 0, SCREEN_WIDTH * 2);
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     Set the X and Y positions of the console cursor simultaneously.
 *
 * PARAMETERS:
 *     X - New X position.
 *     Y - New Y position.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void BmSetCursor(uint16_t X, uint16_t Y) {
    CursorX = X;
    CursorY = Y;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     Save the X and Y positions of the console cursor simultaneously.
 *
 * PARAMETERS:
 *     X - Pointer to save location for the X position.
 *     Y - Pointer to save location for the Y position.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void BmGetCursor(uint16_t *X, uint16_t *Y) {
    *X = CursorX;
    *Y = CursorY;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     Set only the X position of the console cursor.
 *
 * PARAMETERS:
 *     X - New X position.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void BmSetCursorX(uint16_t x) {
    CursorX = x;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     Set only the Y position of the console cursor.
 *
 * PARAMETERS:
 *     Y - New X position.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void BmSetCursorY(uint16_t y) {
    CursorY = y;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     Get only the X position of the console cursor.
 *
 * PARAMETERS:
 *     None.
 *
 * RETURN VALUE:
 *     Current X position.
 *-----------------------------------------------------------------------------------------------*/
uint16_t BmGetCursorX(void) {
    return CursorX;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     Get only the Y position of the console cursor.
 *
 * PARAMETERS:
 *     None.
 *
 * RETURN VALUE:
 *     Current Y position.
 *-----------------------------------------------------------------------------------------------*/
uint16_t BmGetCursorY(void) {
    return CursorY;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     Display a character. This is the internal function, use BmPut() instead.
 *
 * PARAMETERS:
 *     c - The character.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void BiPutChar(char Character) {
    if (Character == '\n') {
        CursorX = 0;
        CursorY++;
    } else if (Character == '\t') {
        CursorX += TAB_SIZE;
    } else {
        VideoMemory[CursorY * SCREEN_WIDTH + CursorX] = SCREEN_ATTRIBUTE << 8 | Character;
        CursorX++;
    }

    if (CursorX >= SCREEN_WIDTH) {
        CursorX = 0;
        CursorY++;
    }

    if (CursorY >= SCREEN_HEIGHT) {
        ScrollUp();
        CursorY = SCREEN_HEIGHT - 1;
    }
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function clears anything that the BIOS or our bootsector has displayed during the boot
 *     process.
 *
 * PARAMETERS:
 *     None.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void BmInitDisplay(void) {
    CursorX = 0;
    CursorY = 0;

    if (!(SCREEN_ATTRIBUTE & 0xF0)) {
        memset(VideoMemory, 0, SCREEN_SIZE * 2);
    } else {
        for (int i = 0; i < SCREEN_SIZE; i++) {
            VideoMemory[i] = SCREEN_ATTRIBUTE << 8;
        }
    }
}
