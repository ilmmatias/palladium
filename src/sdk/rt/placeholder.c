/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#ifdef _DLL
bool __stdcall DllMain(void *hDllHandle, unsigned int dwReason, void *lpreserved) {
    (void)hDllHandle;
    (void)dwReason;
    (void)lpreserved;
    return true;
}
#endif /* _DLL */
