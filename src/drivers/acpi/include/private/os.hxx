/* SPDX-FileCopyrightText: (C) 2023-2024 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#ifndef _OS_HXX_
#define _OS_HXX_

#include <ke.h>

#include <sdt.hxx>

SdtHeader *AcpipFindTable(const char Signature[4], int Index);

void AcpipShowInfoMessage(const char *Format, ...);
void AcpipShowDebugMessage(const char *Format, ...);
void AcpipShowTraceMessage(const char *Format, ...);
[[noreturn]] void AcpipShowErrorMessage(int Reason, const char *Format, ...);

#endif /* _OS_H_ */
