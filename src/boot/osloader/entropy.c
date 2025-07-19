/* SPDX-FileCopyrightText: (C) 2024-2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <console.h>
#include <crt_impl/rand.h>
#include <efi/spec.h>
#include <support.h>

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function initialize the entropy/random number generator source for the memory arena
 *     allocator.
 *
 * PARAMETERS:
 *     None.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void OslpInitializeEntropy(void) {
    /* First attempt the per-architecture entropy source. */
    if (OslpInitializeArchEntropy()) {
        return;
    }

    /* Fallback, try using the UEFI entropy source; If we can't do that, the seed will stay as
     * 0. */
    uint64_t Seed = 0;
    EFI_RNG_PROTOCOL *Rng = NULL;
    if (gBS->LocateProtocol(&gEfiRngProtocolGuid, NULL, (VOID **)Rng) == EFI_SUCCESS &&
        Rng->GetRNG(Rng, NULL, 8, (UINT8 *)&Seed) == EFI_SUCCESS) {
        __srand64(Seed);
        return;
    }

    OslPrint("Failed to initialize the entropy source.\r\n");
    OslPrint("KASLR will be predictable across reboots.\r\n");
}
