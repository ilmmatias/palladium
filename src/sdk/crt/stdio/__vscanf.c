/* SPDX-FileCopyrightText: (C) 2023-2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <ctype.h>
#include <limits.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define MOD_NONE 0
#define MOD_hh 1
#define MOD_h 2
#define MOD_ll 3
#define MOD_l 4
#define MOD_j 5
#define MOD_z 6
#define MOD_t 7
#define MOD_L 8

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     Alternative version of strtoll, supporting intmax_t, and reading character-by-character
 *     instead of expecting the whole string to be already in place.
 *
 * PARAMETERS:
 *     base - Which base the number is in, or 0 for auto detection.
 *     width_set - Set to 1 if we should take into consideration the width.
 *     width - Maximum amount of characters we're allowed to read.
 *     value - Output; Where to store the number on success.
 *     read - I/O; Pointer to an integer storing the `total amount of characters read`.
 *     context - Platform-specific implementation context/detail.
 *     read_ch - What to do when we need to read something; Do not pass a NULL pointer!
 *     unread_ch - What to do when we failed to match something, and we need to unwind the input.
 *
 * RETURN VALUE:
 *     1 on success, 0 on failure.
 *-----------------------------------------------------------------------------------------------*/
static int _strtoi(
    int base,
    int width_set,
    int width,
    intmax_t *value,
    int *read,
    void *context,
    int (*read_ch)(void *context),
    void (*unread_ch)(void *context, int ch)) {
    /* An almost rule-by-rule match with the strtol implementation; We're optimistic, and
       we're guessing it is a valid integer; It should fail by the end (and unwind) if we
       were wrong. Some sections are commented in the original implementation but not here,
       they are functionally equivalent in both (but using read_ch here instead of *(str++)). */

    int ch = read_ch(context);
    while (isspace(ch)) {
        ch = read_ch(context);
        (*read)++;
    }

    intmax_t sign = ch == '-' ? -1 : 1;
    if (sign < 0 || ch == '+') {
        if (width_set && !--width) {
            *value = 0;
            return 0;
        }

        ch = read_ch(context);
        (*read)++;
    }

    int prefix = 0;
    size_t accum = 0;
    int overflow = 0;

    if (base == 0 && ch == '0') {
        if (!width_set || --width) {
            ch = read_ch(context);
            (*read)++;

            if (ch == 'x' || ch == 'X') {
                if (width_set && !--width) {
                    *value = 0;
                    return 1;
                }

                ch = read_ch(context);
                base = 16;
                (*read)++;
            } else {
                base = 8;
                accum++;
            }
        } else {
            *value = 0;
            return 1;
        }

        prefix = 1;
    } else if (base == 0) {
        base = 10;
    }

    if (!prefix && base == 16 && ch == '0') {
        if (!width_set || --width) {
            ch = read_ch(context);
            (*read)++;

            if (ch == 'x' || ch == 'X') {
                if (width_set && !--width) {
                    *value = 0;
                    return 1;
                }

                ch = read_ch(context);
                (*read)++;
            } else {
                accum++;
            }
        } else {
            *value = 0;
            return 1;
        }
    }

    *value = 0;
    while (1) {
        intmax_t last = *value;
        if (ch == EOF) {
            break;
        }

        if (islower(ch)) {
            ch -= 0x57;
        } else if (isupper(ch)) {
            ch -= 0x37;
        } else if (isdigit(ch)) {
            ch -= 0x30;
        } else {
            break;
        }

        if (ch >= base) {
            break;
        }

        /* The other main difference between this and strtoll, is that overflow isn't an error
           here (we just fix the value to INTMAX_MAX). */
        if (!overflow) {
            *value = (*value * base) + ch;
            if (*value < last) {
                *value = INTMAX_MAX;
                overflow = 1;
            }
        }

        accum++;
        (*read)++;

        if (!width_set || --width) {
            ch = read_ch(context);
        } else {
            ch = EOF;
            break;
        }
    }

    unread_ch(context, ch);

    if (overflow) {
        return 1;
    } else if (accum) {
        *value *= sign;
        return 1;
    } else {
        *value = 0;
        return 0;
    }
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     Alternative version of strtoull, supporting uintmax_t, and reading character-by-character
 *     instead of expecting the whole string to be already in place.
 *
 * PARAMETERS:
 *     base - Which base the number is in, or 0 for auto detection.
 *     width_set - Set to 1 if we should take into consideration the width.
 *     width - Maximum amount of characters we're allowed to read.
 *     value - Output; Where to store the number on success.
 *     read - I/O; Pointer to an integer storing the `total amount of characters read`.
 *     context - Platform-specific implementation context/detail.
 *     read_ch - What to do when we need to read something; Do not pass a NULL pointer!
 *     unread_ch - What to do when we failed to match something, and we need to unwind the input.
 *
 * RETURN VALUE:
 *     1 on success, 0 on failure.
 *-----------------------------------------------------------------------------------------------*/
static int _strtou(
    int base,
    int width_set,
    int width,
    uintmax_t *value,
    int *read,
    void *context,
    int (*read_ch)(void *context),
    void (*unread_ch)(void *context, int ch)) {
    int ch = read_ch(context);
    while (isspace(ch)) {
        ch = read_ch(context);
        (*read)++;
    }

    size_t accum = 0;
    int overflow = 0;

    /* Base detection is disabled/absent, as scanf doesn't have an auto-detect specifier for
       unsigned integers. */
    if (base == 16 && ch == '0') {
        if (!width_set || --width) {
            ch = read_ch(context);
            (*read)++;

            if (ch == 'x' || ch == 'X') {
                if (width_set && !--width) {
                    *value = 0;
                    return 1;
                }

                ch = read_ch(context);
                (*read)++;
            } else {
                accum++;
            }
        } else {
            *value = 0;
            return 1;
        }
    }

    *value = 0;
    while (1) {
        uintmax_t last = *value;
        if (ch == EOF) {
            break;
        }

        if (islower(ch)) {
            ch -= 0x57;
        } else if (isupper(ch)) {
            ch -= 0x37;
        } else if (isdigit(ch)) {
            ch -= 0x30;
        } else {
            break;
        }

        if (ch >= base) {
            break;
        }

        if (!overflow) {
            *value = (*value * base) + ch;
            if (*value < last) {
                *value = UINTMAX_MAX;
                overflow = 1;
            }
        }

        accum++;
        (*read)++;

        if (!width_set || --width) {
            ch = read_ch(context);
        } else {
            ch = EOF;
            break;
        }
    }

    unread_ch(context, ch);

    return accum > 0;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function implements our internal formatted input routine.
 *     Do not call it unless you know what you're doing!
 *
 * PARAMETERS:
 *     format - Base format string.
 *     vlist - Variadic argument list.
 *     context - Implementation-defined context.
 *     read_ch - What to do when we need to read something; Do not pass a NULL pointer!
 *     unread_ch - What to do when we failed to match something, and we need to unwind the input.
 *
 * RETURN VALUE:
 *     How many of the variadic arguments we filled.
 *-----------------------------------------------------------------------------------------------*/
int __vscanf(
    const char *format,
    va_list vlist,
    void *context,
    int (*read_ch)(void *context),
    void (*unread_ch)(void *context, int ch)) {
    int filled = 0;
    int read = 0;

    while (*format) {
        int ch = *(format++);

        if (isspace(ch)) {
            ch = *format;
            while (isspace(ch)) {
                ch = *(++format);
            }

            while (1) {
                ch = read_ch(context);

                if (!isspace(ch)) {
                    unread_ch(context, ch);
                    break;
                }

                read++;
            }

            continue;
        }

        if (ch != '%') {
            int in = read_ch(context);
            if (in != ch) {
                unread_ch(context, in);
                return filled;
            }

            read++;
            continue;
        }

        /* Output supressor, doesn't write to any varidic arguemnts when set. */
        int supress = *format == '*';
        if (supress) {
            format++;
        }

        /* Width specifier, use this together with '%s', unless you want a buffer overflow. */
        int width = 0;
        int width_set = 0;
        if (isdigit(*format)) {
            width = strtol(format, (char **)&format, 10);
            width_set = 1;
        }

        /* Final group, length modifiers. */
        int mod = 0;
        switch (*format) {
            case 'h':
                if (*(format + 1) == 'h') {
                    format += 2;
                    mod = MOD_hh;
                } else {
                    format++;
                    mod = MOD_h;
                }
                break;
            case 'l':
                if (*(format + 1) == 'l') {
                    format += 2;
                    mod = MOD_ll;
                } else {
                    format++;
                    mod = MOD_l;
                }
                break;
            case 'j':
                format++;
                mod = MOD_j;
                break;
            case 'z':
                format++;
                mod = MOD_z;
                break;
            case 't':
                format++;
                mod = MOD_t;
                break;
            case 'L':
                format++;
                mod = MOD_L;
                break;
        }

        uintmax_t unsigned_value;
        intmax_t signed_value;
        char *str_start;
        char *str;

        ch = *(format++);

        /* At last, handle the specifiers. */
        switch (ch) {
            case '%':
                ch = read_ch(context);
                if (ch != '%') {
                    unread_ch(context, ch);
                    return filled ? filled : EOF;
                }

                read++;
                break;
            /* Reads exactly 1 or `width` characters; Don't use it if you expect NULL
               termination! */
            case 'c':
                /* TODO: missing wide char support. */
                if (!supress) {
                    str_start = va_arg(vlist, char *);
                    str = str_start;
                }

                width = width_set ? width : 1;
                while (width-- > 0) {
                    int ch = read_ch(context);
                    if (ch == EOF && str - str_start) {
                        break;
                    } else if (ch == EOF) {
                        return filled ? filled : EOF;
                    }

                    if (!supress) {
                        *(str++) = ch;
                    }

                    read++;
                }

                if (!supress) {
                    filled++;
                }

                break;
            /* Reads until whitespace/end, or until width == 0; Buffer size should be width+1,
               as we always write the NULL terminator. */
            case 's':
                /* TODO: missing wide string support. */
                ch = read_ch(context);
                while (isspace(ch)) {
                    ch = read_ch(context);
                    read++;
                }

                if (!supress) {
                    str_start = va_arg(vlist, char *);
                    str = str_start;
                }

                unread_ch(context, ch);
                while (1) {
                    if (width_set && width-- <= 0) {
                        break;
                    }

                    ch = read_ch(context);
                    if (ch == EOF && str - str_start) {
                        break;
                    } else if (ch == EOF) {
                        return filled ? filled : EOF;
                    } else if (isspace(ch)) {
                        unread_ch(context, ch);
                        break;
                    }

                    if (!supress) {
                        *(str++) = ch;
                    }

                    read++;
                }

                if (!supress) {
                    *str = 0;
                    filled++;
                }

                break;
            /* Is there any reason why signed integer specifiers have an auto-base, but not
               unsigned? */
            case 'd':
            case 'i':
                if (!_strtoi(
                        ch == 'd' ? 10 : 0,
                        width_set && width,
                        width,
                        &signed_value,
                        &read,
                        context,
                        read_ch,
                        unread_ch)) {
                    return filled ? filled : EOF;
                }

                if (!supress) {
                    switch (mod) {
                        case MOD_hh:
                            *va_arg(vlist, char *) = signed_value;
                            break;
                        case MOD_h:
                            *va_arg(vlist, short *) = signed_value;
                            break;
                        case MOD_l:
                            *va_arg(vlist, long *) = signed_value;
                            break;
                        case MOD_ll:
                            *va_arg(vlist, long long *) = signed_value;
                            break;
                        case MOD_j:
                            *va_arg(vlist, intmax_t *) = signed_value;
                            break;
                        case MOD_z:
                            *va_arg(vlist, size_t *) = signed_value;
                            break;
                        case MOD_t:
                            *va_arg(vlist, ptrdiff_t *) = signed_value;
                            break;
                        default:
                            *va_arg(vlist, int *) = signed_value;
                            break;
                    }

                    filled++;
                }

                break;
            case 'u':
            case 'o':
            case 'x':
            case 'X':
                if (!_strtou(
                        ch == 'u'   ? 10
                        : ch == 'o' ? 8
                                    : 16,
                        width_set,
                        width,
                        &unsigned_value,
                        &read,
                        context,
                        read_ch,
                        unread_ch)) {
                    return filled ? filled : EOF;
                }

                if (!supress) {
                    switch (mod) {
                        case MOD_hh:
                            *va_arg(vlist, unsigned char *) = unsigned_value;
                            break;
                        case MOD_h:
                            *va_arg(vlist, unsigned short *) = unsigned_value;
                            break;
                        case MOD_l:
                            *va_arg(vlist, unsigned long *) = unsigned_value;
                            break;
                        case MOD_ll:
                            *va_arg(vlist, unsigned long long *) = unsigned_value;
                            break;
                        case MOD_j:
                            *va_arg(vlist, uintmax_t *) = unsigned_value;
                            break;
                        case MOD_z:
                            *va_arg(vlist, size_t *) = unsigned_value;
                            break;
                        case MOD_t:
                            *va_arg(vlist, ptrdiff_t *) = unsigned_value;
                            break;
                        default:
                            *va_arg(vlist, unsigned int *) = unsigned_value;
                            break;
                    }

                    filled++;
                }

                break;
            case 'n':
                if (!supress) {
                    *va_arg(vlist, int *) = read;
                }

                break;
            case 'p':
                if (!_strtou(
                        16,
                        width,
                        width_set,
                        &unsigned_value,
                        &read,
                        context,
                        read_ch,
                        unread_ch)) {
                    return filled ? filled : EOF;
                }

                if (!supress) {
                    *va_arg(vlist, void **) = (void *)unsigned_value;
                    filled++;
                }

                break;
            default:
                return filled;
        }
    }

    return filled;
}
