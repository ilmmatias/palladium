/* SPDX-FileCopyrightText: (C) 2023-2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <crt_impl/common.h>
#include <string.h>

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function implements generic code to find the next set of characters after a delimeter
 *     from the set.
 *
 * PARAMETERS:
 *     s1 - Either a string to analyze, or NULL to use the previous context.
 *     s2 - Delimiter set.
 *
 * RETURN VALUE:
 *     Position of the next token, or NULL if there are no more tokens.
 *-----------------------------------------------------------------------------------------------*/
char *strtok(char *CRT_RESTRICT s1, const char *CRT_RESTRICT s2) {
    static char *Context = NULL;

    if (!Context || s1) {
        Context = s1;
    }

    if (!Context) {
        return NULL;
    }

    Context += strspn(Context, s2);
    if (!*Context) {
        Context = NULL;
        return NULL;
    }

    char *Token = Context;
    Context += strcspn(Context, s2);

    if (*Context) {
        *(Context++) = 0;
    } else {
        Context = NULL;
    }

    return Token;
}
