/* SPDX-FileCopyrightText: (C) 2023 yuuma03
 * SPDX-License-Identifier: BSD-3-Clause */

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
#endif
