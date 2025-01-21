/* SPDX-FileCopyrightText: (C) 2023-2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#ifndef _KI_H_
#define _KI_H_

#include <hal.h>
#include <ke.h>

#define KI_ACPI_NONE 0
#define KI_ACPI_RDST 1
#define KI_ACPI_XSDT 2

typedef struct {
    char Magic[4];
    uint64_t LoaderVersion;
    RtDList *MemoryDescriptorListHead;
    RtDList *BootDriverListHead;
    void *AcpiTable;
    uint32_t AcpiTableVersion;
    void *BackBuffer;
    void *FrontBuffer;
    uint32_t FramebufferWidth;
    uint32_t FramebufferHeight;
    uint32_t FramebufferPitch;
} KiLoaderBlock;

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

void KiSaveAcpiData(KiLoaderBlock *LoaderBlock);

void KiSaveBootStartDrivers(KiLoaderBlock *BootData);
void KiRunBootStartDrivers(void);
void KiDumpSymbol(void *Address);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _KI_H_ */
