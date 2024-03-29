/* SPDX-FileCopyrightText: (C) 2023-2024 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

.text

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function makes a previous LGDT take action, and reloads all segments.
 *
 * PARAMETERS:
 *     None.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
.global HalpFlushGdt
HalpFlushGdt:
    movabs $0f, %rax
    push $0x08
    push %rax
    lretq
0:
    mov $0x10, %ax
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %ss
    mov %ax, %fs
    mov %ax, %gs
    ret
