/* SPDX-FileCopyrightText: (C) 2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#ifndef _OS_AMD64_INTRIN_H_
#define _OS_AMD64_INTRIN_H_

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function reads the specified MSR leaf.
 *
 * PARAMETERS:
 *     Number - Which leaf we should read.
 *
 * RETURN VALUE:
 *     What RDMSR returned.
 *-----------------------------------------------------------------------------------------------*/
static inline uint64_t ReadMsr(uint32_t Number) {
    uint32_t LowPart;
    uint32_t HighPart;
    __asm__ volatile("rdmsr" : "=a"(LowPart), "=d"(HighPart) : "c"(Number));
    return LowPart | ((uint64_t)HighPart << 32);
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function writes into the specified MSR leaf.
 *
 * PARAMETERS:
 *     Number - Which leaf we should write into.
 *     Value - What we want to write.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
static inline void WriteMsr(uint32_t Number, uint64_t Value) {
    __asm__ volatile("wrmsr" : : "a"((uint32_t)Value), "c"(Number), "d"(Value >> 32));
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function signals the CPU we're in a busy loop (waiting for a spin lock probably), and
 *     we are safe to drop the CPU state into power saving.
 *
 * PARAMETERS:
 *     None.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
static inline void PauseProcessor(void) {
    __asm__ volatile("pause");
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function signals the CPU we're either fully done with any work this CPU will ever do
 *     (aka, panic), or that we're waiting some interrupt/external event.
 *
 * PARAMETERS:
 *     None.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
static inline void StopProcessor(void) {
    __asm__ volatile("hlt");
}

/* Wrappers around in/out functions; All in* functions take a single argument (the port number),
   and return what we've read from the port, while the out* functions also take the value to be
   written. */

static inline uint8_t ReadPortByte(uintptr_t Port) {
    uint8_t Result;
    __asm__ volatile("inb %1, %0" : "=a"(Result) : "Nd"((uint16_t)Port) : "memory");
    return Result;
}

static inline uint16_t ReadPortWord(uintptr_t Port) {
    uint16_t Result;
    __asm__ volatile("inw %1, %0" : "=a"(Result) : "Nd"((uint16_t)Port) : "memory");
    return Result;
}

static inline uint32_t ReadPortDWord(uintptr_t Port) {
    uint32_t Result;
    __asm__ volatile("inl %1, %0" : "=a"(Result) : "Nd"((uint16_t)Port) : "memory");
    return Result;
}

static inline void WritePortByte(uintptr_t Port, uint8_t Data) {
    __asm__ volatile("outb %0, %1" : : "a"(Data), "Nd"((uint16_t)Port) : "memory");
}

static inline void WritePortWord(uintptr_t Port, uint16_t Data) {
    __asm__ volatile("outw %0, %1" : : "a"(Data), "Nd"((uint16_t)Port) : "memory");
}

static inline void WritePortDWord(uintptr_t Port, uint32_t Data) {
    __asm__ volatile("outl %0, %1" : : "a"(Data), "Nd"((uint16_t)Port) : "memory");
}

#endif /* _OS_AMD64_INTRIN_H_ */
