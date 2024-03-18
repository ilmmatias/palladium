/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#ifndef _OS_H_
#define _OS_H_

#include <sdt.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

SdtHeader *AcpipFindTable(const char Signature[4], int Index);

void AcpipShowInfoMessage(const char *Format, ...);
void AcpipShowDebugMessage(const char *Format, ...);
void AcpipShowTraceMessage(const char *Format, ...);
[[noreturn]] void AcpipShowErrorMessage(int Reason, const char *Format, ...);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _OS_H_ */
