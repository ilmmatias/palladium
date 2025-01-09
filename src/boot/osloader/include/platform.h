/* SPDX-FileCopyrightText: (C) 2024-2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#ifndef _PLATFORM_H_
#define _PLATFORM_H_

#include <rt/list.h>

#define OSLP_BOOT_MAGIC "OLDR"
#define OSLP_BOOT_VERSION 0x0000'0000'00000001

typedef struct {
    char Magic[4];
    uint64_t LoaderVersion;
    RtDList *MemoryDescriptorListHead;
    RtDList *BootDriverListHead;
    void *BootProcessor;
    void *AcpiTable;
    uint32_t AcpiTableVersion;
    void *Framebuffer;
    uint32_t FramebufferWidth;
    uint32_t FramebufferHeight;
    uint32_t FramebufferPitch;
} OslpBootBlock;

void *OslpInitializeBsp(void);
int OslpPrepareExecution(RtDList *LoadedPrograms, RtDList *MemoryDescriptors);
[[noreturn]] void OslpTransferExecution(OslpBootBlock *BootBlock);

#endif /* _PLATFORM_H_ */
