/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#ifndef _X86_TIMER_H_
#define _X86_TIMER_H_

#include <stdint.h>

#define PORT_REG 0x70
#define PORT_DATA 0x71

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

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function writes a byte to an IO port.
 *
 * PARAMETERS:
 *     Port - Which port to write into.
 *
 * RETURN VALUE:
 *     Result of `inb`.
 *-----------------------------------------------------------------------------------------------*/
static void WritePort(uint16_t Port, uint8_t Data) {
    __asm__ volatile("outb %0, %1" :: "a"(Data), "Nd"(Port) : "memory");
}

#endif /* _X86_TIMER_H_ */
