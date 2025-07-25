/* SPDX-FileCopyrightText: (C) 2023-2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <cpuid.h>
#include <kernel/halp.h>
#include <kernel/intrin.h>
#include <kernel/ke.h>
#include <kernel/vid.h>
#include <string.h>

/* Macro to help with checking the feature bits from CPUID (just to make the code a bit more
 * compact). */
#define CHECK_FEATURE(Feature, Register, Bit) \
    if ((Register) & (1 << (Bit))) {          \
        HalpPlatformFeatures |= (Feature);    \
    }

[[noreturn]] extern void KiSystemStartup(KiLoaderBlock *LoaderBlock, KeProcessor *Processor);

/* This is pretty important to be here, as we need it before the memory manager initialization
 * to switch out of the bootloader stack. */
static KeProcessor BootProcessor __attribute__((aligned(4096))) = {};

char HalpPlatformManufacturerString[12] = {};
char HalpPlatformProcessorBrandString[48] = {};
uint32_t HalpPlatformMaxLeaf = 0;
uint32_t HalpPlatformMaxExtendedLeaf = 0;
uint64_t HalpPlatformFeatures = 0;

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function collects data from CPUID to fill in the processor name/manufacturer.
 *
 * PARAMETERS:
 *     None.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
static void CollectManufacturer(void) {
    /* Leaf 0 contains both the max supported leaf (not counting the extended leafs) and the basic
     * 12 characters long manufacturer ID, so read in that first. */
    uint32_t Eax, Ebx, Ecx, Edx;
    __cpuid(HALP_CPUID_MAX_LEAF, HalpPlatformMaxLeaf, Ebx, Ecx, Edx);
    memcpy(HalpPlatformManufacturerString, (void *)&Ebx, 4);
    memcpy(HalpPlatformManufacturerString + 4, (void *)&Edx, 4);
    memcpy(HalpPlatformManufacturerString + 8, (void *)&Ecx, 4);

    /* And now attempt to extract the actual processor name from the extended leaf. */
    __cpuid(HALP_CPUID_MAX_EXTENDED_LEAF, HalpPlatformMaxExtendedLeaf, Ebx, Ecx, Edx);
    if (HalpPlatformMaxExtendedLeaf >= HALP_CPUID_EXTENDED_PROCESSOR_BRAND(2)) {
        for (int i = 0; i < 3; i++) {
            __cpuid(HALP_CPUID_EXTENDED_PROCESSOR_BRAND(i), Eax, Ebx, Ecx, Edx);
            memcpy(HalpPlatformProcessorBrandString + i * 16, (void *)&Eax, 4);
            memcpy(HalpPlatformProcessorBrandString + i * 16 + 4, (void *)&Ebx, 4);
            memcpy(HalpPlatformProcessorBrandString + i * 16 + 8, (void *)&Ecx, 4);
            memcpy(HalpPlatformProcessorBrandString + i * 16 + 12, (void *)&Edx, 4);
        }
    } else {
        strcpy(HalpPlatformProcessorBrandString, "(unavailable)");
    }

    VidPrint(
        VID_MESSAGE_DEBUG,
        "Kernel HAL",
        "cpu manufacturer: %.12s\n",
        HalpPlatformManufacturerString);

    VidPrint(VID_MESSAGE_INFO, "Kernel HAL", "cpu name: %.48s\n", HalpPlatformProcessorBrandString);
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function collects data from CPUID to fill in the internal feature mask.
 *
 * PARAMETERS:
 *     None.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
static void CollectFeatures(void) {
    uint32_t Eax, Ebx, Ecx, Edx;

    if (HalpPlatformMaxLeaf >= HALP_CPUID_PROCESSOR_INFO) {
        __cpuid(HALP_CPUID_PROCESSOR_INFO, Eax, Ebx, Ecx, Edx);
        CHECK_FEATURE(HALP_FEATURE_SSE3, Ecx, 0);
        CHECK_FEATURE(HALP_FEATURE_PCLMULQDQ, Ecx, 1);
        CHECK_FEATURE(HALP_FEATURE_MONITOR, Ecx, 3);
        CHECK_FEATURE(HALP_FEATURE_VMX, Ecx, 5);
        CHECK_FEATURE(HALP_FEATURE_SSSE3, Ecx, 9);
        CHECK_FEATURE(HALP_FEATURE_FMA, Ecx, 12);
        CHECK_FEATURE(HALP_FEATURE_CX16, Ecx, 13);
        CHECK_FEATURE(HALP_FEATURE_PCID, Ecx, 17);
        CHECK_FEATURE(HALP_FEATURE_SSE41, Ecx, 19);
        CHECK_FEATURE(HALP_FEATURE_SSE42, Ecx, 20);
        CHECK_FEATURE(HALP_FEATURE_X2APIC, Ecx, 21);
        CHECK_FEATURE(HALP_FEATURE_MOVBE, Ecx, 22);
        CHECK_FEATURE(HALP_FEATURE_POPCNT, Ecx, 23);
        CHECK_FEATURE(HALP_FEATURE_AES_NI, Ecx, 25);
        CHECK_FEATURE(HALP_FEATURE_XSAVE, Ecx, 26);
        CHECK_FEATURE(HALP_FEATURE_AVX, Ecx, 28);
        CHECK_FEATURE(HALP_FEATURE_F16C, Ecx, 29);
        CHECK_FEATURE(HALP_FEATURE_RDRAND, Ecx, 30);
        CHECK_FEATURE(HALP_FEATURE_HYPERVISOR, Ecx, 31);
        CHECK_FEATURE(HALP_FEATURE_TSC, Edx, 4);
        CHECK_FEATURE(HALP_FEATURE_SSE, Edx, 25);
        CHECK_FEATURE(HALP_FEATURE_SSE2, Edx, 26);
    }

    if (HalpPlatformMaxLeaf >= HALP_CPUID_EXTENDED_FEATURES) {
        __get_cpuid_count(HALP_CPUID_EXTENDED_FEATURES, 0, &Eax, &Ebx, &Ecx, &Edx);
        CHECK_FEATURE(HALP_FEATURE_FSGSBASE, Ebx, 0);
        CHECK_FEATURE(HALP_FEATURE_BMI1, Ebx, 3);
        CHECK_FEATURE(HALP_FEATURE_AVX2, Ebx, 5);
        CHECK_FEATURE(HALP_FEATURE_SMEP, Ebx, 7);
        CHECK_FEATURE(HALP_FEATURE_BMI2, Ebx, 8);
        CHECK_FEATURE(HALP_FEATURE_ERMS, Ebx, 9);
        CHECK_FEATURE(HALP_FEATURE_INVPCID, Ebx, 10);
        CHECK_FEATURE(HALP_FEATURE_RDSEED, Ebx, 18);
        CHECK_FEATURE(HALP_FEATURE_SMAP, Ebx, 20);
        CHECK_FEATURE(HALP_FEATURE_SHA, Ebx, 29);
        CHECK_FEATURE(HALP_FEATURE_UMIP, Ecx, 2);
        CHECK_FEATURE(HALP_FEATURE_WAITPKG, Ecx, 5);
        CHECK_FEATURE(HALP_FEATURE_GFNI, Ecx, 8);
        CHECK_FEATURE(HALP_FEATURE_LA57, Ecx, 16);
        CHECK_FEATURE(HALP_FEATURE_RDPID, Ecx, 22);
        CHECK_FEATURE(HALP_FEATURE_MOVDIRI, Ecx, 27);
        CHECK_FEATURE(HALP_FEATURE_MOVDIR64B, Ecx, 28);
        CHECK_FEATURE(HALP_FEATURE_HYBRID, Edx, 15);

        __get_cpuid_count(HALP_CPUID_EXTENDED_FEATURES, 1, &Eax, &Ebx, &Ecx, &Edx);
        CHECK_FEATURE(HALP_FEATURE_SHA512, Ebx, 0);
        CHECK_FEATURE(HALP_FEATURE_CMPCCXADD, Ebx, 7);
        CHECK_FEATURE(HALP_FEATURE_FRED, Ebx, 17);
        CHECK_FEATURE(HALP_FEATURE_LKGS, Ebx, 18);
        CHECK_FEATURE(HALP_FEATURE_APX, Edx, 11);
    }

    if (HalpPlatformMaxExtendedLeaf >= HALP_CPUID_EXTENDED_PROCESSOR_INFO) {
        __cpuid(HALP_CPUID_EXTENDED_PROCESSOR_INFO, Eax, Ebx, Ecx, Edx);
        CHECK_FEATURE(HALP_FEATURE_LZCNT, Ecx, 5);
        CHECK_FEATURE(HALP_FEATURE_PDPE_1GB, Edx, 26);
        CHECK_FEATURE(HALP_FEATURE_RDTSCP, Edx, 27);
    }

    if (HalpPlatformMaxExtendedLeaf >= HALP_CPUID_PPM_INFO) {
        __cpuid(HALP_CPUID_PPM_INFO, Eax, Ebx, Ecx, Edx);
        CHECK_FEATURE(HALP_FEATURE_INVARIANT_TSC, Edx, 8);
    }

    VidPrint(VID_MESSAGE_DEBUG, "Kernel HAL", "cpu feature mask: %016llx\n", HalpPlatformFeatures);
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function re-enters KiSystemStartup using the boot processor kernel stack instead of the
 *     osloader temporary stack.
 *
 * PARAMETERS:
 *     LoaderBlock - Data prepared by the boot loader for us.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void HalpInitializeBootStack(KiLoaderBlock *LoaderBlock) {
    /* The BSP gets its KeGetCurrentProcessor() initialized here instead of
     * HalpInitializeXProcessor (because we need it for early KeFatalError).
     * Maybe we should create a function specifically for this? */
    WriteMsr(HALP_MSR_KERNEL_GS_BASE, (uint64_t)&BootProcessor);
    BootProcessor.StackBase = BootProcessor.SystemStack;
    BootProcessor.StackLimit = BootProcessor.SystemStack + sizeof(BootProcessor.SystemStack);

    __asm__ volatile("mov %0, %%rax\n"
                     "mov %1, %%rcx\n"
                     "mov %2, %%rdx\n"
                     "mov %3, %%rsp\n"
                     "jmp *%%rax\n"
                     :
                     : "r"(KiSystemStartup),
                       "r"(LoaderBlock),
                       "r"(&BootProcessor),
                       "r"(BootProcessor.SystemStack + sizeof(BootProcessor.SystemStack) - 8)
                     : "%rax", "%rcx", "%rdx");

    while (true) {
        StopProcessor();
    }
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function runs any early arch-specific initialization routines required for the boot
 *     processor.
 *
 * PARAMETERS:
 *     None.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void HalpInitializeBootProcessor(void) {
    CollectManufacturer();
    CollectFeatures();
    HalpInitializeGdt(&BootProcessor);
    HalpInitializeIdt(&BootProcessor);
    HalpInitializeIoapic();
    HalpInitializeApic();
    HalpEnableApic();
    BootProcessor.ApicId = HalpReadLapicId();
    HalpInitializeHpet();
    HalpInitializeTsc();
    HalpInitializeTimer();
    HalpInitializeSmp();
    HalpInitializeApicTimer();
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function runs any early arch-specific initialization routines required for the
 *     secondary/application processors.
 *
 * PARAMETERS:
 *     Processor - Pointer to the processor-specific structure.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void HalpInitializeApplicationProcessor(KeProcessor *Processor) {
    WriteMsr(HALP_MSR_KERNEL_GS_BASE, (uint64_t)Processor);
    Processor->StackBase = Processor->SystemStack;
    Processor->StackLimit = Processor->SystemStack + sizeof(Processor->SystemStack);
    HalpInitializeGdt(Processor);
    HalpInitializeIdt(Processor);
    HalpEnableApic();
    Processor->ApicId = HalpReadLapicId();
    HalpInitializeApicTimer();
}
