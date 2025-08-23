/* SPDX-FileCopyrightText: (C) 2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#ifndef _KERNEL_DETAIL_KDPINLINE_H_
#define _KERNEL_DETAIL_KDPINLINE_H_

#include <kernel/detail/kdinline.h>

/* clang-format off */
#if __has_include(ARCH_MAKE_INCLUDE_PATH(kernel/detail, kdpinline.h))
#include ARCH_MAKE_INCLUDE_PATH(kernel/detail, kdpinline.h)
#endif /* __has__include */
/* clang-format on */

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function swaps the given word from network order into the host integer order
 *     (or vice-versa).
 *
 * PARAMETERS:
 *     Value - What we should swap.
 *
 * RETURN VALUE:
 *     Swapped value.
 *-----------------------------------------------------------------------------------------------*/
static inline uint16_t KdpSwapNetworkOrder16(uint16_t Value) {
#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
    return Value;
#elif __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    return __builtin_bswap16(Value);
#else
#error "Undefined byte order for the kernel module"
#endif
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function swaps the given dword from network order into the host integer order
 *     (or vice-versa).
 *
 * PARAMETERS:
 *     Value - What we should swap.
 *
 * RETURN VALUE:
 *     Swapped value.
 *-----------------------------------------------------------------------------------------------*/
static inline uint32_t KdpSwapNetworkOrder32(uint32_t Value) {
#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
    return Value;
#elif __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    return __builtin_bswap32(Value);
#else
#error "Undefined byte order for the kernel module"
#endif
}

#endif /* _KERNEL_DETAIL_KDPINLINE_H_ */
