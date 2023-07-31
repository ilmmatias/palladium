/* SPDX-FileCopyrightText: (C) 2023 yuuma03
 * SPDX-License-Identifier: BSD-3-Clause */

#include <boot.h>

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     Internal assertion failed routine, do not call directly! Shows what happened and halts the
 *     system after an expected condition evaluated to false.
 *
 * PARAMETERS:
 *     condition - Which condition failed to evaluate.
 *     file - Which file the assert() was called.
 *     line - Line within the file.
 *     func - Function within the file.
 *
 * RETURN VALUE:
 *     Does not return.
 *-----------------------------------------------------------------------------------------------*/
[[noreturn]] void __assert(const char *condition, const char *file, int line, const char *func) {
    BmPut("%s:%d: %s: Assertion `%s` failed.", file, line, func, condition);
    while (1)
        ;
}
