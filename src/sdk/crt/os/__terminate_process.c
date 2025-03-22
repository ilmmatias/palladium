/* SPDX-FileCopyrightText: (C) 2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function terminates the current process (and all of its childs).
 *
 * PARAMETERS:
 *     res - Which result code to terminate with.
 *
 * RETURN VALUE:
 *     Does not return!
 *-----------------------------------------------------------------------------------------------*/
[[noreturn]] void __terminate_process(int res) {
    (void)res;
    while (true) {
    }
}
