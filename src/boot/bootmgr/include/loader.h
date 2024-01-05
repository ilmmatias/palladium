/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#ifndef _LOADER_H_
#define _LOADER_H_

#include <config.h>

void BiZeroRequiredSections(void);
void BiReserveLoaderSections(void);

void BiCheckCompatibility(int Type);
[[noreturn]] void BiLoadEntry(BmMenuEntry *Entry);
[[noreturn]] void BiLoadPalladium(BmMenuEntry *Entry);
[[noreturn]] void BiLoadChainload(BmMenuEntry *Entry);

#endif /* _LOADER_H_ */
