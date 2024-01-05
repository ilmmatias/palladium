/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#ifndef _X86_KEYBOARD_H_
#define _X86_KEYBOARD_H_

#include <stdint.h>

#define STATUS_HAS_OUTPUT 0x01

#define PORT_DATA 0x60
#define PORT_STATUS 0x64
#define PORT_COMMAND 0x64

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function reads a byte from an IO port.
 *
 * PARAMETERS:
 *     Port - Which port to read from.
 *
 * RETURN VALUE:
 *     Result of `inb`.
 *-----------------------------------------------------------------------------------------------*/
static uint8_t ReadPort(uint16_t Port) {
    uint8_t Result;
    __asm__ volatile("inb %1, %0" : "=a"(Result) : "Nd"(Port) : "memory");
    return Result;
}

#endif /* _X86_KEYBOARD_H_ */
