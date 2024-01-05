/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#ifdef _DLL
bool __stdcall DllMainCRTStartup(void *hDllHandle, unsigned int dwReason, void *lpreserved) {
    (void)hDllHandle;
    (void)dwReason;
    (void)lpreserved;
    return true;
}

bool __stdcall CRTDLL_INIT(void *hDllHandle, unsigned int dwReason, void *lpreserved) {
    (void)hDllHandle;
    (void)dwReason;
    (void)lpreserved;
    return true;
}
#endif /* _DLL */
