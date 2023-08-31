/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#include <crt_impl.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <vid.h>

/* stdio START; for debugging our AML interpreter. */
static void put_buf(const void *buffer, int size, void *context) {
    (void)context;
    for (int i = 0; i < size; i++) {
        VidPutChar(((const char *)buffer)[i]);
    }
}

static int printf(const char *format, ...) {
    va_list vlist;
    va_start(vlist, format);
    int len = __vprintf(format, vlist, NULL, put_buf);
    va_end(vlist);
    return len;
}
/* stdio END */

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function parses a PkgLength element.
 *
 * PARAMETERS:
 *     Code - Pointer to the current position along the AML code.
 *     Length - Pointer to the remaining length of the code.
 *
 * RETURN VALUE:
 *     Length of the package (scope, function, etc).
 *-----------------------------------------------------------------------------------------------*/
static uint32_t ParsePkgLength(const uint8_t **Code, uint32_t *Length) {
    if (!*Length) {
        return 0;
    }

    /* High 2 bits of the leading byte reveals how many bytes the package length itself is
       composed of. */
    uint8_t Type = *((*Code)++);
    (*Length)--;

    uint32_t Result = 0;
    switch (Type >> 6) {
        /* For 0, the other 6 bits (of the leading byte) are used. */
        case 0:
            Result = Type & 0x3F;
            break;
        /* Otherwise, only the first 4 bits are used, followed by N whole bytes, where N is how
           many bytes those 2 high bits told us to use. */
        case 3:
            if (*Length) {
                Result |= (uint32_t)(*((*Code)++)) << 20;
                (*Length)--;
            }
        case 2:
            if (*Length) {
                Result |= (uint32_t)(*((*Code)++)) << 12;
                (*Length)--;
            }
        case 1:
            if (*Length) {
                Result |= (uint32_t)(*((*Code)++)) << 4;
                (*Length)--;
            }

            Result |= Type & 0x0F;
            break;
    }

    return Result;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function parses a name string/segment (according to the AML spec).
 *
 * PARAMETERS:
 *     Code - Pointer to the current position along the AML code.
 *     Length - Pointer to the remaining length of the code.
 *
 * RETURN VALUE:
 *     Pointer to the output string, allocated with malloc (the caller is expected to manage it
 *     manually!); Or NULL if either the allocation failed, or we reached the end of the input
 *     buffer.
 *-----------------------------------------------------------------------------------------------*/
static char *ParseNameString(const uint8_t **Code, uint32_t *Length) {
    if (!*Length) {
        return NULL;
    }

    /* We should always be prefixed by either the root character (\) or 0+ `parent scope`
       characters (^). */
    int Prefixes = **Code == '\\';
    int Root = 0;

    if (Prefixes) {
        (*Length)--;
        (*Code)++;
        Root = 1;
    } else {
        while (**Code == '^' && *Length) {
            (*Length)--;
            (*Code)++;
            Prefixes++;
        }
    }

    /* We're now followed by either a NullName (0x00, aka, nothing); 2 name segments (prefixed by
       0x2E); 3+ name segments (0x2F); Or just a single name segment. */
    int SegCount = 0;

    if (*Length) {
        if (!**Code) {
            (*Length)--;
            (*Code)++;
        } else if (**Code == 0x2E) {
            (*Length)--;
            (*Code)++;
            SegCount = 2;
        } else if (**Code == 0x2F) {
            (*Code)++;
            if ((*Length)--) {
                SegCount = *((*Code)++);
                (*Length)--;
            }
        } else {
            SegCount = 1;
        }
    } else if (!Prefixes) {
        return NULL;
    }

    char *NameString = malloc(Prefixes + SegCount * 4 + (SegCount ? SegCount : 1));
    int NeedsSeparator = 0;
    int NamePos = 0;

    if (!NameString) {
        return 0;
    } else if (Prefixes && Root) {
        NameString[NamePos++] = '\\';
    } else if (Prefixes) {
        while (Prefixes--) {
            NameString[NamePos++] = '^';
        }
    }

    while (*Length > 4 && SegCount--) {
        if (NeedsSeparator) {
            NameString[NamePos++] = '.';
        } else {
            NeedsSeparator = 1;
        }

        memcpy(&NameString[NamePos], *Code, 4);
        NamePos += 4;
        *Length -= 4;
        *Code += 4;
    }

    NameString[NamePos] = 0;
    return NameString;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function populates the AML namespace tree using the given DSDT/SSDT.
 *
 * PARAMETERS:
 *     Code - Pointer to where the AML code starts.
 *     Length - Length of the code (without the SDT header!).
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void AcpipPopulateTree(const uint8_t *Code, uint32_t Length) {
    (void)ParsePkgLength;
    (void)ParseNameString;
    printf("AcpipPopulateTree(%p, %u)\n", Code, Length);
}
