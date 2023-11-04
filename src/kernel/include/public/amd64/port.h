/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#ifndef _AMD64_PORT_H_
#define _AMD64_PORT_H_

#include <stdint.h>

/* Wrappers around in/out functions; All in* functions take a single argument (the port number),
   and return what we've read from the port, while the out* functions also take the value to be
   written. */

static inline uint8_t ReadPortByte(uint16_t Port) {
    uint8_t Result;
    __asm__ volatile("inb %1, %0" : "=a"(Result) : "Nd"(Port) : "memory");
    return Result;
}

static inline uint16_t ReadPortWord(uint16_t Port) {
    uint16_t Result;
    __asm__ volatile("inw %1, %0" : "=a"(Result) : "Nd"(Port) : "memory");
    return Result;
}

static inline uint32_t ReadPortDWord(uint16_t Port) {
    uint32_t Result;
    __asm__ volatile("inl %1, %0" : "=a"(Result) : "Nd"(Port) : "memory");
    return Result;
}

static inline void WritePortByte(uint16_t Port, uint8_t Data) {
    __asm__ volatile("outb %0, %1" ::"a"(Data), "Nd"(Port) : "memory");
}

static inline void WritePortWord(uint16_t Port, uint16_t Data) {
    __asm__ volatile("outw %0, %1" ::"a"(Data), "Nd"(Port) : "memory");
}

static inline void WritePortDWord(uint16_t Port, uint32_t Data) {
    __asm__ volatile("outl %0, %1" ::"a"(Data), "Nd"(Port) : "memory");
}

#endif /* _AMD64_PORT_H_ */
