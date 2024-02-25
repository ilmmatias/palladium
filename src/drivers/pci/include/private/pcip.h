/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#ifndef _PCIP_H_
#define _PCIP_H_

#include <acpi.h>
#include <rt/list.h>

typedef struct {
    RtSList ListHeader;
    AcpiObject *Object;
    uint64_t Seg;
    uint64_t Bbn;
} PcipBus;

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

void PcipShowInfoMessage(const char *Format, ...);
[[noreturn]] void PcipShowErrorMessage(int Code, const char *Format, ...);

void PcipInitializeBus(PcipBus *Bus);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _PCIP_H_ */
