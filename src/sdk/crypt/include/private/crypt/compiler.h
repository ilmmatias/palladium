/* SPDX-FileCopyrightText: (C) 2025-2026 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#ifndef _CRYPT_COMPILER_H_
#define _CRYPT_COMPILER_H_

/* We should always be using clang for the build (which is guaranteed to have
 * __builtin_memcpy/memset_inline in newer versions), but let's also add a fallback
 * just in case. */

#if __has_builtin(__builtin_memcpy_inline)
#define MEMCPY_INLINE(s1, s2, n) __builtin_memcpy_inline((s1), (s2), (n))
#else
#define MEMCPY_INLINE(s1, s2, n) __builtin_memcpy((s1), (s2), (n))
#endif

#if __has_builtin(__builtin_memset_inline)
#define MEMSET_INLINE(s, c, n) __builtin_memset_inline((s), (c), (n))
#else
#define MEMSET_INLINE(s, c, n) __builtin_memset((s), (c), (n))
#endif

#define COMPILER_BARRIER(s) __asm__ volatile("" : : "g"(&(s)) : "memory")

#define BSWAP32(A) __builtin_bswap32((A))
#define BSWAP64(A) __builtin_bswap64((A))

#if __has_builtin(__builtin_rotateleft32)
#define ROTL32(A, B) __builtin_rotateleft32((A), (B))
#elif __has_builtin(__builtin_stdc_rotate_left)
#define ROTL32(A, B) __builtin_stdc_rotate_left((A), (B))
#else
#define ROTL32(A, B) (((A) << (B & 0x1f)) | ((A) >> ((32 - B) & 0x1f)))
#endif

#if __has_builtin(__builtin_rotateleft64)
#define ROTL64(A, B) __builtin_rotateleft64((A), (B))
#elif __has_builtin(__builtin_stdc_rotate_left)
#define ROTL64(A, B) __builtin_stdc_rotate_left((A), (B))
#else
#define ROTL64(A, B) (((A) << (B & 0x3f)) | ((A) >> ((64 - B) & 0x3f)))
#endif

#if __has_builtin(__builtin_rotateright32)
#define ROTR32(A, B) __builtin_rotateright32((A), (B))
#elif __has_builtin(__builtin_stdc_rotate_right)
#define ROTR32(A, B) __builtin_stdc_rotate_right((A), (B))
#else
#define ROTR32(A, B) (((A) >> (B & 0x1f)) | ((A) << ((32 - B) & 0x1f)))
#endif

#if __has_builtin(__builtin_rotateright64)
#define ROTR64(A, B) __builtin_rotateright64((A), (B))
#elif __has_builtin(__builtin_stdc_rotate_right)
#define ROTR64(A, B) __builtin_stdc_rotate_right((A), (B))
#else
#define ROTR64(A, B) (((A) >> (B & 0x3f)) | ((A) << ((64 - B) & 0x3f)))
#endif

#endif /* _CRYPT_COMPILER_H_ */
