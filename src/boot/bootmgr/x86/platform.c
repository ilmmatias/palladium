/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <boot.h>
#include <crt_impl.h>
#include <display.h>
#include <memory.h>
#include <stdio.h>
#include <string.h>
#include <x86/bios.h>
#include <x86/cpuid.h>

#define BASE_MESSAGE "Your device does not support one or more of the required features "

typedef struct __attribute__((aligned)) {
    char Signature[8];
    uint8_t Checksum;
    char OemId[6];
    uint8_t Revision;
    uint32_t RsdtAddress;
    uint32_t Length;
    uint64_t XsdtAddress;
    uint8_t ExtendedChecksum;
    uint8_t Reserved[3];
} RsdpHeader;

extern BiMemoryArenaEntry *BiMemoryArena;
extern int BiMemoryArenaSize;

static BiMemoryArenaEntry KernelRegion[512];

uint64_t BiosRsdtLocation = 0;
int BiosTableType = ACPI_NONE;

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function searches for the RSDP signature within the specified region.
 *
 * PARAMETERS:
 *     Region - Start of the search region.
 *     End - End of the region (we'll access at most it minus 1).
 *     BiosTableType - Output; ACPI_XSDT if we have ACPI 2.0+, ACPI_RSDT for ACPI 1.0.
 *
 * RETURN VALUE:
 *     Pointer to the RSDT/XSDT if it was found within the region, NULL otherwise.
 *-----------------------------------------------------------------------------------------------*/
static uint64_t SearchRsdp(char *Region, char *End, int *BiosTableType) {
    while (1) {
        if (Region >= End) {
            return 0;
        } else if (memcmp(Region, "RSD PTR ", 8)) {
            /* According to the spec, the signature should always be in a 16-byte aligned
               section. */
            Region += 16;
            continue;
        }

        break;
    }

    RsdpHeader *Rsdp = (RsdpHeader *)Region;
    uint8_t Checksum = 0;
    uint64_t Location;

    /* The checksum can be calculated by summing all bytes of the struct; It should always equal
       zero. */
    if (!Rsdp->Revision) {
        for (int i = 0; i < 20; i++) {
            Checksum += Region[i];
        }

        Location = Rsdp->RsdtAddress;
        *BiosTableType = ACPI_RSDT;
    } else {
        for (uint32_t i = 0; i < Rsdp->Length; i++) {
            Checksum += Region[i];
        }

        Location = Rsdp->XsdtAddress;
        *BiosTableType = ACPI_XSDT;
    }

    return Checksum ? 0 : Location;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function sets up any remaining architecture-dependent features.
 *
 * PARAMETERS:
 *     None.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void BiInitializePlatform(void) {
    /* The virtual region allocators expects us to feed it with all possible regions inside the
       arena; We're using 1GiB regions (limited to 512 images in total), fill all entries based
       on that. */
    for (uint64_t i = 0; i < 512; i++) {
        KernelRegion[i].Base = BI_ARENA_BASE + (i << 30);
        KernelRegion[i].Next = i == 511 ? NULL : &KernelRegion[i + 1];
    }

    BiMemoryArena = KernelRegion;
    BiMemoryArenaSize = 512;

    /* RDSEED is seemingly a non determistic RNG, and we can use that to seed the PRNG on any new
       enough computer (it's VERY slow, so we should only use it as seed).
       RDRAND is a bit more supported, but gives no direct access to the hardware RNG.
       TSC/cycle counter is the last option, and should be supported on pretty much everything. */

    uint32_t SeedLow = 1, SeedHigh = 0;
    uint32_t Eax, Ebx, Ecx, Edx;

    do {
        __get_cpuid_count(7, 0, &Eax, &Ebx, &Ecx, &Edx);
        if (Ebx & bit_RDSEED) {
            __asm__ volatile("rdseed %0" : "=r"(SeedLow));
            __asm__ volatile("rdseed %0" : "=r"(SeedHigh));
            break;
        }

        __cpuid(1, Eax, Ebx, Ecx, Edx);
        if (Ecx & bit_RDRND) {
            __asm__ volatile("rdrand %0" : "=r"(SeedLow));
            __asm__ volatile("rdrand %0" : "=r"(SeedHigh));
        } else if (Edx & bit_TSC) {
            __asm__ volatile("rdtsc" : "=a"(SeedLow), "=d"(SeedHigh));
        }
    } while (0);

    __srand64(((uint64_t)SeedHigh << 32) | SeedLow);
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function validates if the host computer is capable of running the specified OS.
 *
 * PARAMETERS:
 *     Type - Which OS we're trying to load.
 *
 * RETURN VALUE:
 *     None; Does not return if the host is incompatible.
 *-----------------------------------------------------------------------------------------------*/
void BiCheckCompatibility(int Type) {
    /* At the moment, the two features we care the most are XSAVE and LM (which implies SSE2). */
    (void)Type;

    uint32_t Eax, Ebx, Ecx, Edx;

    __cpuid(1, Eax, Ebx, Ecx, Edx);
    if (!(Ecx & bit_XSAVE)) {
        BmPrint(BASE_MESSAGE "(XSAVE).\n");
        while (1)
            ;
    }

    __cpuid(0x80000001, Eax, Ebx, Ecx, Edx);
    if (!(Edx & bit_LM)) {
        BmPrint(BASE_MESSAGE "(LM).\n");
        while (1)
            ;
    }

    /* All seems well; Palladium requires ACPI support, if we already have the RSDP saved
       somewhere, that's nice, but otherwise, search around/close to the EBDA area. */

    if (BiosRsdtLocation) {
        return;
    }

    /* This entry of the BDA USUALLY contains the EBDA base; Assume that if it is lower than the
       other BIOS area we'll manually search, we can check the first 1KiBs of it. */
    char *EbdaArea = (char *)((uint32_t)(*(uint16_t *)0x40E) << 4);

    if (EbdaArea < (char *)0x100000) {
        BiosRsdtLocation = SearchRsdp(EbdaArea, EbdaArea + 1024, &BiosTableType);
    }

    if (!BiosRsdtLocation) {
        BiosRsdtLocation = SearchRsdp((char *)0xE0000, (char *)0x100000, &BiosTableType);
    }

    if (!BiosRsdtLocation) {
        BmPrint(BASE_MESSAGE "(ACPI).\n");
        while (1)
            ;
    }
}
