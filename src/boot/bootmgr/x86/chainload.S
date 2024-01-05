/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

.text

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function returns the processor to a post "the BIOS just loaded the boot sector" state,
 *     and transfers execution to 07C0:0000h.
 *
 * PARAMETERS:
 *     BootDrive (esp + 4) - Which BIOS drive we give the bootsector on DL.
 *
 * RETURN VALUE:
 *     Does not return.
 *-----------------------------------------------------------------------------------------------*/
.global _BiJumpChainload
_BiJumpChainload:
    mov 4(%esp), %al
    mov %al, (.LBootDrive + 1)

    xor %eax, %eax
    push %ax
    lea .LReal, %eax
    push %ax
    lea .LProtected16, %eax
    push $0x18
    push %eax
    lret

.code16
.LProtected16:
    mov $0x20, %ax
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %fs
    mov %ax, %gs
    mov %ax, %ss

    /* IDT should still be in place, disable the PE bit and drop to real mode. */
    mov %cr0, %eax
    and $0xFE, %al
    mov %eax, %cr0

    lret

.LReal:
    xor %ax, %ax
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %fs
    mov %ax, %gs
    mov %ax, %ss
    sti

    /* Reset to text mode (we're in VESA/graphics atm). */
    xor %ah, %ah
    mov $3, %al
    int $0x10

.LBootDrive:
    mov $0, %dl

    /* Transfer executon to the boot sector. */ 
    ljmp $0x7C0, $0
