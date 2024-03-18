/* SPDX-FileCopyrightText: (C) 2023-2024 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#ifndef _CXX_NEW_HXX_
#define _CXX_NEW_HXX_

#include <stdint.h>

inline void *operator new(size_t, void *Pointer) {
    return Pointer;
}

#endif /* _CXX_NEW_HXX_ */
