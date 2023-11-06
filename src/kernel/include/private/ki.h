/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#ifndef _KI_H_
#define _KI_H_

#include <ke.h>

void KiNotifyProcessors(void);

void KiRunBootStartDrivers(void *LoaderData);

#endif /* _KI_H_ */
