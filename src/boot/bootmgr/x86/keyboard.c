/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#include <display.h>
#include <keyboard.h>
#include <stdint.h>
#include <x86/keyboard.h>

/* Scan Code Set 1 (the old PC-XT keyboard scan codes, which the controller should be emulating
   by default). */
static int LowercaseScanCodeSet1[89] = {
    KEY_UNKNOWN, KEY_ESC, '1',         '2',    '3',         '4',         '5',         '6',
    '7',         '8',     '9',         '0',    '-',         '=',         '\b',        '\t',
    'q',         'w',     'e',         'r',    't',         'y',         'u',         'i',
    'o',         'p',     '[',         ']',    '\n',        KEY_UNKNOWN, 'a',         's',
    'd',         'f',     'g',         'h',    'j',         'k',         'l',         ';',
    '\'',        '`',     KEY_UNKNOWN, '\\',   'z',         'x',         'c',         'v',
    'b',         'n',     'm',         ',',    '.',         '/',         KEY_UNKNOWN, '*',
    KEY_UNKNOWN, ' ',     KEY_UNKNOWN, KEY_F1, KEY_F2,      KEY_F3,      KEY_F4,      KEY_F5,
    KEY_F6,      KEY_F7,  KEY_F8,      KEY_F9, KEY_F10,     KEY_UNKNOWN, KEY_UNKNOWN, '7',
    '8',         '9',     '-',         '4',    '5',         '6',         '+',         '1',
    '2',         '3',     '0',         '.',    KEY_UNKNOWN, KEY_UNKNOWN, KEY_UNKNOWN, KEY_F11,
    KEY_F12,
};

static int UppercaseScanCodeSet1[89] = {
    KEY_UNKNOWN, KEY_ESC, '!',         '@',    '#',         '$',         '%',         '^',
    '&',         '*',     '(',         ')',    '_',         '+',         '\b',        '\t',
    'Q',         'W',     'E',         'R',    'T',         'Y',         'U',         'I',
    'O',         'P',     '{',         '}',    '\n',        KEY_UNKNOWN, 'A',         'S',
    'D',         'F',     'G',         'H',    'J',         'K',         'L',         ':',
    '"',         '~',     KEY_UNKNOWN, '|',    'Z',         'X',         'C',         'V',
    'B',         'N',     'M',         '<',    '>',         '?',         KEY_UNKNOWN, '*',
    KEY_UNKNOWN, ' ',     KEY_UNKNOWN, KEY_F1, KEY_F2,      KEY_F3,      KEY_F4,      KEY_F5,
    KEY_F6,      KEY_F7,  KEY_F8,      KEY_F9, KEY_F10,     KEY_UNKNOWN, KEY_UNKNOWN, '7',
    '8',         '9',     '-',         '4',    '5',         '6',         '+',         '1',
    '2',         '3',     '0',         '.',    KEY_UNKNOWN, KEY_UNKNOWN, KEY_UNKNOWN, KEY_F11,
    KEY_F12,
};

static int ExtendedScanCodeSet1[80] = {
    KEY_UNKNOWN, KEY_UNKNOWN, KEY_UNKNOWN, KEY_UNKNOWN, KEY_UNKNOWN, KEY_UNKNOWN, KEY_UNKNOWN,
    KEY_UNKNOWN, KEY_UNKNOWN, KEY_UNKNOWN, KEY_UNKNOWN, KEY_UNKNOWN, '\n',        KEY_UNKNOWN,
    KEY_UNKNOWN, KEY_UNKNOWN, KEY_UNKNOWN, KEY_UNKNOWN, KEY_UNKNOWN, KEY_UNKNOWN, KEY_UNKNOWN,
    KEY_UNKNOWN, KEY_UNKNOWN, KEY_UNKNOWN, KEY_UNKNOWN, KEY_UNKNOWN, KEY_UNKNOWN, KEY_UNKNOWN,
    KEY_UNKNOWN, KEY_UNKNOWN, KEY_UNKNOWN, KEY_UNKNOWN, KEY_UNKNOWN, KEY_UNKNOWN, KEY_UNKNOWN,
    KEY_UNKNOWN, KEY_UNKNOWN, '/',         KEY_UNKNOWN, KEY_UNKNOWN, KEY_UNKNOWN, KEY_UNKNOWN,
    KEY_UNKNOWN, KEY_UNKNOWN, KEY_UNKNOWN, KEY_UNKNOWN, KEY_UNKNOWN, KEY_UNKNOWN, KEY_UNKNOWN,
    KEY_UNKNOWN, KEY_UNKNOWN, KEY_UNKNOWN, KEY_UNKNOWN, KEY_UNKNOWN, KEY_UNKNOWN, KEY_HOME,
    KEY_UP,      KEY_PGUP,    KEY_UNKNOWN, KEY_LEFT,    KEY_UNKNOWN, KEY_RIGHT,   KEY_UNKNOWN,
    KEY_END,     KEY_DOWN,    KEY_PGDOWN,  KEY_INS,     KEY_DEL,     KEY_UNKNOWN, KEY_UNKNOWN,
    KEY_UNKNOWN, KEY_UNKNOWN, KEY_UNKNOWN, KEY_UNKNOWN, KEY_UNKNOWN, KEY_UNKNOWN, KEY_UNKNOWN,
    KEY_UNKNOWN, KEY_UNKNOWN, KEY_UNKNOWN,
};

static int CapsLockActive = 0;
static int LeftShiftPressed = 0;
static int RightShiftPressed = 0;

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function tries polling the data output port of the PS/2 controller.
 *     This has no distinction between the first and second ports!
 *
 * PARAMETERS:
 *     None.
 *
 * RETURN VALUE:
 *     Output of the data port.
 *-----------------------------------------------------------------------------------------------*/
static uint8_t PollData(void) {
    while (!(ReadPort(PORT_STATUS) & STATUS_HAS_OUTPUT))
        ;
    return ReadPort(PORT_DATA);
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function sets up the PS/2 keyboard input.
 *
 * PARAMETERS:
 *     None.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void BiInitKeyboard(void) {
    /* TODO: We need a proper init sequence, resetting the controller and detecting multiple PS/2
       devices (and which one is the keyboard). */
    ReadPort(PORT_DATA);
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function tries polling the PS/2 controller for the next key, and converts it using
 *     the scan code table.
 *
 * PARAMETERS:
 *     None.
 *
 * RETURN VALUE:
 *     Keyboard input.
 *-----------------------------------------------------------------------------------------------*/
int BmPollKey(void) {
    while (1) {
        int ScanCode = PollData();

        /* Manually handle shift+caps lock (both onpress and onrelease); Those should be
           transparently handled by us (there is no KEY_LSHIFT_PRESS or KEY_LSHIFT_RELEASE). */
        if (ScanCode == 0x2A) {
            LeftShiftPressed = 1;
            continue;
        } else if (ScanCode == 0x36) {
            RightShiftPressed = 1;
            continue;
        } else if (ScanCode == 0x3A) {
            CapsLockActive = ~CapsLockActive;
            continue;
        } else if (ScanCode == 0xAA) {
            LeftShiftPressed = 0;
            continue;
        } else if (ScanCode == 0xB6) {
            RightShiftPressed = 0;
            continue;
        }

        /* E0 is the extended scan code prefix; We mostly support what's already in the normal
           table, plus arrow keys. */
        if (ScanCode == 0xE0) {
            ScanCode = PollData();

            if (ScanCode > 0x90) {
                continue;
            }

            return ScanCode < 16 || ScanCode > 95 ? KEY_UNKNOWN
                                                  : ExtendedScanCodeSet1[ScanCode - 16];
        }

        /* Other than E0, anything higher than 0x80 is a key release, which we don't handle. */
        if (ScanCode > 0x80) {
            continue;
        } else if (ScanCode > 88) {
            return KEY_UNKNOWN;
        }

        int ShiftPressed = LeftShiftPressed || RightShiftPressed;
        return ShiftPressed ^ CapsLockActive ? UppercaseScanCodeSet1[ScanCode]
                                             : LowercaseScanCodeSet1[ScanCode];
    }
}
