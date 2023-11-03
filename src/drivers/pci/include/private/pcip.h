/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#ifndef _PCIP_H_
#define _PCIP_H_

#include <acpi.h>
#include <rt.h>

/* The flags below control how much debugging info should be displayed on boot.
   On normal debugging, you probably want only PCI_ENABLE_INFO.  */
#define PCI_ENABLE_INFO 1
#define PCI_ENABLE_TRACE 0

typedef struct {
    RtSList ListHeader;
    AcpiObject *Object;
    uint64_t Seg;
    uint64_t Bbn;
} PcipBus;

void PcipShowInfoMessage(const char *Format, ...);
[[noreturn]] void PcipShowErrorMessage(int Code, const char *Format, ...);

void PcipInitializeBus(PcipBus *Bus);

#endif /* _PCIP_H_ */
