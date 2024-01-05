/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#ifndef _EVP_H_
#define _EVP_H_

#include <ev.h>

void EvpDispatchObject(void *Object, uint64_t Timeout, int Yield);

#endif /* _EVP_H_ */
