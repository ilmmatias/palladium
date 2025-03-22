/* SPDX-FileCopyrightText: (C) 2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#ifndef LIMITS_H
#define LIMITS_H

#define __STDC_VERSION_LIMITS_H__ 202311L

#define BOOL_WIDTH __BOOL_WIDTH__

#define SCHAR_WIDTH __CHAR_BIT__
#define SCHAR_MIN (-__SCHAR_MAX__ - 1)
#define SCHAR_MAX __SCHAR_MAX__

#define UCHAR_WIDTH __CHAR_BIT__
#define UCHAR_MAX (__SCHAR_MAX__ * 2U + 1U)

#define CHAR_BIT __CHAR_BIT__
#define CHAR_WIDTH __CHAR_BIT__
#ifdef __CHAR_UNSIGNED__
#define CHAR_MIN SCHAR_MIN
#define CHAR_MAX SCHAR_MAX
#else
#define CHAR_MIN 0
#define CHAR_MAX UCHAR_MAX
#endif

#define SHRT_WIDTH __SHRT_WIDTH__
#define SHRT_MIN (-__SHRT_MAX__ - 1)
#define SHRT_MAX __SHRT_MAX__

#define USHRT_WIDTH __SHRT_WIDTH__
#define USHRT_MAX (__SHRT_MAX__ * 2U + 1U)

#define INT_WIDTH __INT_WIDTH__
#define INT_MIN (-__INT_MAX__ - 1)
#define INT_MAX __INT_MAX__

#define UINT_WIDTH __INT_WIDTH__
#define UINT_MAX (__INT_MAX__ * 2U + 1U)

#define LONG_WIDTH __LONG_WIDTH__
#define LONG_MIN (-__LONG_MAX__ - 1L)
#define LONG_MAX __LONG_MAX__

#define ULONG_WIDTH __LONG_WIDTH__
#define ULONG_MAX (__LONG_MAX__ * 2UL + 1Ul)

#ifdef __LLONG_WIDTH__
#define LLONG_WIDTH __LLONG_WIDTH__
#define ULLONG_WIDTH __LLONG_WIDTH__
#else
#define LLONG_WIDTH __LONG_LONG_WIDTH__
#define ULLONG_WIDTH __LONG_LONG_WIDTH__
#endif

#define LLONG_MIN (-__LONG_LONG_MAX__ - 1LL)
#define LLONG_MAX __LONG_LONG_MAX__

#define ULLONG_MAX (__LONG_LONG_MAX__ * 2ULL + 1ULL)

#define BITINT_MAXWIDTH __BITINT_MAXWIDTH__

/* What exactly is the right value for this? */
#define MB_LEN_MAX 16

#endif /* LIMITS_H */
