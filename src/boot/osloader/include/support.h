/* SPDX-FileCopyrightText: (C) 2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#ifndef _SUPPORT_H_
#define _SUPPORT_H_

#include <platform.h>

bool OslpInitializeArchEntropy(void);
bool OslpCheckArchSupport(void);
void OslpInitializeArchBootData(OslpBootBlock* BootBlock);

#endif /* _SUPPORT_H_ */
