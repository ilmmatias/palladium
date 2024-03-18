/* SPDX-FileCopyrightText: (C) 2023-2024 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#ifdef _DLL
bool __stdcall DllMain(void *hDllHandle, unsigned int dwReason, void *lpreserved) {
    (void)hDllHandle;
    (void)dwReason;
    (void)lpreserved;
    return true;
}
#endif /* _DLL */
