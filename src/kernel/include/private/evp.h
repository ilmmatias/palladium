/* SPDX-FileCopyrightText: (C) 2023-2024 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#ifndef _EVP_H_
#define _EVP_H_

#include <ev.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

void EvpDispatchObject(void *Object, uint64_t Timeout, int Yield);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _EVP_H_ */
