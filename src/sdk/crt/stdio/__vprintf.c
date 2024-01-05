/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <ctype.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

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
 *     This function pads the output, either before or after outputting the specified buffer.
 *
 * PARAMETERS:
 *     buffer - Output buffer.
 *     size - Output buffer size.
 *     left - Controls if we need to left-align (0), or right-align (1).
 *     width - Minimum width specifier.
 *     prec - Maximum width specifier.
 *     context - Implementation-defined context.
 *     put_buf - What to do when we need to output something; Do not pass a NULL pointer!
 *
 * RETURN VALUE:
 *     How many characters have been output.
 *-----------------------------------------------------------------------------------------------*/
static int pad(
    const void *buffer,
    int size,
    int left,
    int width,
    int prec,
    void *context,
    void (*put_buf)(const void *buffer, int size, void *context)) {
    if (prec >= 0 && size > prec) {
        size = prec;
    }

    if (width > size) {
        width -= size;
    } else {
        width = 0;
    }

    int pad = ' ';
    int written_size = width;

    while (!left && width-- > 0) {
        put_buf(&pad, 1, context);
    }

    put_buf(buffer, size, context);

    while (left && width-- > 0) {
        put_buf(&pad, 1, context);
    }

    return written_size + size;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function properly pads the output buffer for outputting a number.
 *
 * PARAMETERS:
 *     buffer - What to output.
 *     size - Size of the output data.
 *     sign - Controls if we should display a sign (either '-' or ' '), or not (0).
 *     alt - Controls if we should use the octal alt form ('o'), hex alt form ('x' or 'X'), or
 *           just the normal form (0).
 *     left - Controls if we need to left-align (0), or right-align (1).
 *     zero - Controls if the pad character is a space (0), or a zero (1).
 *     width - Minimum width specifier.
 *     prec - Maximum width specifier.
 *     context - Implementation-defined context.
 *     put_buf - What to do when we need to output something; Do not pass a NULL pointer!
 *
 * RETURN VALUE:
 *     How many characters have been output.
 *-----------------------------------------------------------------------------------------------*/
static int pad_num(
    const void *buffer,
    int size,
    int sign,
    int alt,
    int left,
    int zero,
    int width,
    int prec,
    void *context,
    void (*put_buf)(const void *buffer, int size, void *context)) {
    int space_width = 0;
    int space_ch = ' ';
    int zero_width = 0;
    int zero_ch = '0';
    int alt_width = 0;
    char alt_ch[2];

    /* Alternative form for octal takes action when the left most digit isn't a zero,
       while alt form for hex takes action always. */
    if (alt == 'o' && ((char *)buffer)[0] != '0') {
        alt_width = 1;
        alt_ch[0] = '0';
    } else if (alt == 'x' || alt == 'X') {
        alt_width = 2;
        alt_ch[0] = '0';
        alt_ch[1] = alt;
    }

    /* Rules for max width vs only min width are somewhat different.
     *     For max width, prec/max indicates how many characters should we aim to have between the
     *     sign/alt and the number, padding is always spaces.
     *     For left=1, it doesn't matter if the user requested zeroes instead of spaces.
     *     Otherwise, it works just as you would expect. */
    if (prec >= 0) {
        int max_size = (prec > size ? prec : size) + (sign > 0) + alt_width;

        if (width > max_size) {
            space_width = width - max_size;
        }

        zero_width = prec - size;
    } else if (left || !zero) {
        int max_size = size + (sign > 0) + alt_width;

        if (width > max_size) {
            space_width = width - max_size;
        }
    } else {
        int max_size = size + (sign > 0) + alt_width;

        if (width > max_size) {
            zero_width = width - max_size;
        }
    }

    int written_size = space_width + zero_width + alt_width + (sign > 0);

    while (!left && space_width-- > 0) {
        put_buf(&space_ch, 1, context);
    }

    if (sign > 0) {
        put_buf(&sign, 1, context);
    }

    if (alt_width > 0) {
        put_buf(alt_ch, alt_width, context);
    }

    while (zero_width-- > 0) {
        put_buf(&zero_ch, 1, context);
    }

    put_buf(buffer, size, context);

    while (left && space_width-- > 0) {
        put_buf(&space_ch, 1, context);
    }

    return written_size + size;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function converts a signed integer to a string, outputting the result, and applying
 *     all specified flags and alignment.
 *
 * PARAMETERS:
 *     value - What to convert and output.
 *     sign - Controls if we should use a ' ' or '+' when there's not sign.
 *     left - Controls if we need to left-align (0), or right-align (1).
 *     zero - Controls if the pad character is a space (0), or a zero (1).
 *     width - Minimum width specifier.
 *     prec - Maximum width specifier.
 *     context - Implementation-defined context.
 *     put_buf - What to do when we need to output something; Do not pass a NULL pointer!
 *
 * RETURN VALUE:
 *     How many characters have been output.
 *-----------------------------------------------------------------------------------------------*/
static int itoa(
    intmax_t value,
    int sign,
    int left,
    int zero,
    int width,
    int prec,
    void *context,
    void (*put_buf)(const void *buffer, int size, void *context)) {
    int value_sign = value < 0;
    char buffer[64];
    int size = 0;
    int pos = 63;

    if (value_sign) {
        value *= -1;
    }

    while (value) {
        buffer[--pos] = value % 10 + '0';
        value /= 10;
        size++;
    }

    if (!size) {
        buffer[--pos] = '0';
        size++;
    }

    return pad_num(
        buffer + pos, size, value_sign ? '-' : sign, 0, left, zero, width, prec, context, put_buf);
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function converts an unsigned integer to a string, outputting the result, and applying
 *     all specified flags and alignment.
 *     We also properly handle alternative forms, and different bases.
 *
 * PARAMETERS:
 *     value - What to convert and output.
 *     base - Which base should we output it.
 *     upper - Control if we should use uppercase letters for digits >=10.
 *     alt - Controls if we should use the alternative form.
 *     left - Controls if we need to left-align (0), or right-align (1).
 *     zero - Controls if the pad character is a space (0), or a zero (1).
 *     width - Minimum width specifier.
 *     prec - Maximum width specifier.
 *     context - Implementation-defined context.
 *     put_buf - What to do when we need to output something; Do not pass a NULL pointer!
 *
 * RETURN VALUE:
 *     How many characters have been output.
 *-----------------------------------------------------------------------------------------------*/
static int utoa(
    uintmax_t value,
    int base,
    int upper,
    int alt,
    int left,
    int zero,
    int width,
    int prec,
    void *context,
    void (*put_buf)(const void *buffer, int size, void *context)) {
    char buffer[64];
    int size = 0;
    int pos = 63;

    while (value) {
        buffer[--pos] = (upper ? "0123456789ABCDEF" : "0123456789abcdef")[value % base];
        value /= base;
        size++;
    }

    if (!size) {
        buffer[--pos] = '0';
        size++;
    }

    if (alt) {
        if (base == 8) {
            alt = 'o';
        } else if (base == 16) {
            alt = upper ? 'X' : 'x';
        }
    }

    return pad_num(buffer + pos, size, 0, alt, left, zero, width, prec, context, put_buf);
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function implements our internal formatted output routine.
 *     Do not call it unless you know what you're doing!
 *
 * PARAMETERS:
 *     format - Base format string.
 *     vlist - Variadic argument list.
 *     context - Implementation-defined context.
 *     put_buf - What to do when we need to output something; Do not pass a NULL pointer!
 *
 * RETURN VALUE:
 *     How many characters have been output.
 *-----------------------------------------------------------------------------------------------*/
int __vprintf(
    const char *format,
    va_list vlist,
    void *context,
    void (*put_buf)(const void *buffer, int size, void *context)) {
    int size = 0;

    while (*format) {
        const char *start = format;
        int ch = *(format++);

        if (ch != '%') {
            put_buf(&ch, 1, context);
            size++;
            continue;
        }

        /* First group, options can appear in any order (and we don't care about repeats). */
        int sign = 0;
        int left = 0;
        int alt = 0;
        int zero = 0;
        while (1) {
            int stop = 0;

            switch (*format) {
                case '-':
                    left = 1;
                    break;
                case '+':
                    sign = '+';
                    break;
                case ' ':
                    if (sign == 0) {
                        sign = ' ';
                    }
                    break;
                case '#':
                    alt = 1;
                    break;
                case '0':
                    zero = 1;
                    break;
                default:
                    stop = 1;
                    break;
            }

            if (stop) {
                break;
            }

            format++;
        }

        /* Second group, (min) width specifier. */
        int width = 0;
        if (isdigit(*format)) {
            width = strtol(format, (char **)&format, 10);
        } else if (*format == '*') {
            width = va_arg(vlist, int);
            format++;
        }

        /* Third group, precision/max width specifier. */
        int prec = -1;
        if (*format == '.') {
            if (isdigit(*(++format))) {
                prec = strtol(format, (char **)&format, 10);
            } else if (*format == '*') {
                prec = va_arg(vlist, int);
                format++;
            }
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
        const char *str;

        ch = *(format++);

        /* At last, handle the conversion specifiers. */
        switch (ch) {
            case '%':
                put_buf(format - 1, 1, context);
                size++;
                break;
            case 'c':
                /* TODO: missing wide char support. */
                ch = va_arg(vlist, int);
                size += pad(&ch, 1, left, width, -1, context, put_buf);
                break;
            case 's':
                /* TODO: missing wide string support. */
                str = va_arg(vlist, char *);
                size += pad(str, strlen(str), left, width, prec, context, put_buf);
                break;
            case 'd':
            case 'i':
                switch (mod) {
                    case MOD_hh:
                        signed_value = (char)va_arg(vlist, int);
                        break;
                    case MOD_h:
                        signed_value = (short)va_arg(vlist, int);
                        break;
                    case MOD_l:
                        signed_value = va_arg(vlist, long);
                        break;
                    case MOD_ll:
                        signed_value = va_arg(vlist, long long);
                        break;
                    case MOD_j:
                        signed_value = va_arg(vlist, intmax_t);
                        break;
                    case MOD_z:
                        signed_value = va_arg(vlist, size_t);
                        break;
                    case MOD_t:
                        signed_value = va_arg(vlist, ptrdiff_t);
                        break;
                    default:
                        signed_value = va_arg(vlist, int);
                        break;
                }

                size += itoa(signed_value, sign, left, zero, width, prec, context, put_buf);

                break;
            case 'o':
            case 'x':
            case 'X':
            case 'u':
                switch (mod) {
                    case MOD_hh:
                        unsigned_value = (unsigned char)va_arg(vlist, int);
                        break;
                    case MOD_h:
                        unsigned_value = (unsigned short)va_arg(vlist, int);
                        break;
                    case MOD_l:
                        unsigned_value = va_arg(vlist, unsigned long);
                        break;
                    case MOD_ll:
                        unsigned_value = va_arg(vlist, unsigned long long);
                        break;
                    case MOD_j:
                        unsigned_value = va_arg(vlist, uintmax_t);
                        break;
                    case MOD_z:
                        unsigned_value = va_arg(vlist, size_t);
                        break;
                    case MOD_t:
                        unsigned_value = va_arg(vlist, ptrdiff_t);
                        break;
                    default:
                        unsigned_value = va_arg(vlist, unsigned int);
                        break;
                }

                size += utoa(
                    unsigned_value,
                    ch == 'o'                ? 8
                    : ch == 'x' || ch == 'X' ? 16
                                             : 10,
                    ch == 'X',
                    alt,
                    left,
                    zero,
                    width,
                    prec,
                    context,
                    put_buf);

                break;
            case 'n':
                switch (mod) {
                    case MOD_hh:
                        *va_arg(vlist, char *) = size;
                        break;
                    case MOD_h:
                        *va_arg(vlist, short *) = size;
                        break;
                    case MOD_l:
                        *va_arg(vlist, long *) = size;
                        break;
                    case MOD_ll:
                        *va_arg(vlist, long long *) = size;
                        break;
                    case MOD_j:
                        *va_arg(vlist, intmax_t *) = size;
                        break;
                    case MOD_z:
                        *va_arg(vlist, size_t *) = size;
                        break;
                    case MOD_t:
                        *va_arg(vlist, ptrdiff_t *) = size;
                        break;
                    default:
                        *va_arg(vlist, int *) = size;
                        break;
                }

                break;
            case 'p':
                size += utoa(
                    (uintmax_t)va_arg(vlist, void *),
                    16,
                    0,
                    'X',
                    left,
                    zero,
                    width,
                    prec,
                    context,
                    put_buf);
                break;
            default:
                put_buf(start, format - start, context);
                size += format - start;
                break;
        }
    }

    return size;
}
