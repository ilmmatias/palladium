/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#ifndef _AMD64_KI_H_
#define _AMD64_KI_H_

#include <ki.h>

void KiInitializeIdt(void);
void KiInitializeApic(void);
void KiSendEoi(void);

#endif /* _AMD64_KI_H_ */
