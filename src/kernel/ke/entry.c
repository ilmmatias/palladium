/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <amd64/halp.h>
#include <ki.h>
#include <mi.h>
#include <psp.h>
#include <stdlib.h>
#include <string.h>
#include <vidp.h>

// ####### GRAPHICS BACKEND #######

#define BACKGROUND_COLOR 0x09090F

extern char *VidpBackBuffer;
extern uint16_t VidpWidth;
extern uint16_t VidpHeight;
extern uint16_t VidpPitch;

static void PlotPixel(uint32_t Color, int X, int Y) {
    if (X < 0 || Y < 0 || X >= VidpWidth || Y >= VidpHeight) {
        return;
    }

    *(uint32_t *)(VidpBackBuffer + Y * VidpPitch + X * 4) = Color;
}

// ####### UTILITY LIBRARY #######

static uint32_t Rand(void) {
    return rand();
}

typedef int64_t FixedPoint;

#define FIXED_POINT 16  // fixedvalue = realvalue * 2^FIXED_POINT

#define FP_TO_INT(FixedPoint) ((int)((FixedPoint) >> FIXED_POINT))
#define INT_TO_FP(Int) ((FixedPoint)(Int) << FIXED_POINT)

#define MUL_FP_FP(Fp1, Fp2) (((Fp1) * (Fp2)) >> FIXED_POINT)

// Gets a random representing between 0. and 1. in fixed point.
static FixedPoint RandFP(void) {
    return Rand() % (1 << FIXED_POINT);
}

static FixedPoint RandFPSign(void) {
    if (Rand() % 2) {
        return -RandFP();
    }

    return RandFP();
}

#include "sintab.h"

static FixedPoint Sin(int Angle) {
    return INT_TO_FP(SinTable[Angle % 65536]) / 32768;
}

static FixedPoint Cos(int Angle) {
    return Sin(Angle + 16384);
}

// ####### FIREWORK IMPLEMENTATION #######

typedef struct {
    int X, Y;
    uint32_t Color;
    FixedPoint ActX, ActY;  // fixed point
    FixedPoint VelX, VelY;  // fixed point
    int ExplosionRange;
} FireworkData;

static uint32_t GetRandomColor() {
    return (Rand() + 0x808080) & 0xFFFFFF;
}

static void PerformDelay(int Ms) {
    EvTimer Timer;
    EvInitializeTimer(&Timer, Ms * EV_MILLISECS, NULL);
    EvWaitObject(&Timer, 0);
}

static void SpawnParticle(FireworkData *Data);

#define DELTA_SEC Delay / 1000

[[noreturn]] static void T_Particle(void *Parameter) {
    FireworkData *ParentData = Parameter;
    FireworkData Data;
    memset(&Data, 0, sizeof(Data));

    // Inherit position information
    Data.X = ParentData->X;
    Data.Y = ParentData->Y;
    Data.ActX = ParentData->ActX;
    Data.ActY = ParentData->ActY;
    int ExplosionRange = ParentData->ExplosionRange;

    MmFreePool(ParentData, "FWKS");

    int Angle = Rand() % 65536;
    Data.VelX = MUL_FP_FP(Cos(Angle), RandFPSign()) * ExplosionRange;
    Data.VelY = MUL_FP_FP(Sin(Angle), RandFPSign()) * ExplosionRange;

    int ExpireIn = 2000 + Rand() % 1000;
    Data.Color = GetRandomColor();

    int T = 0;
    for (int i = 0; i < ExpireIn;) {
        PlotPixel(Data.Color, Data.X, Data.Y);

        int Delay = 16 + (T != 0);
        PerformDelay(Delay);
        i += Delay;
        T++;
        if (T == 3) {
            T = 0;
        }

        PlotPixel(BACKGROUND_COLOR, Data.X, Data.Y);

        // Update the particle
        Data.ActX += Data.VelX * DELTA_SEC;
        Data.ActY += Data.VelY * DELTA_SEC;

        Data.X = FP_TO_INT(Data.ActX);
        Data.Y = FP_TO_INT(Data.ActY);

        // Gravity
        Data.VelY += INT_TO_FP(10) * DELTA_SEC;
    }

    // Done!
    PsTerminateThread();
}

[[noreturn]] static void T_Explodable(void *) {
    FireworkData Data;
    memset(&Data, 0, sizeof(Data));

    int OffsetX = VidpWidth * 400 / 1024;

    // This is a fire, so it doesn't have a base.
    Data.X = VidpWidth / 2;
    Data.Y = VidpHeight - 1;
    Data.ActX = INT_TO_FP(Data.X);
    Data.ActY = INT_TO_FP(Data.Y);
    Data.VelY = -INT_TO_FP(400 + Rand() % 400);
    Data.VelX = OffsetX * RandFPSign();
    Data.Color = GetRandomColor();
    Data.ExplosionRange = Rand() % 100 + 100;

    int ExpireIn = 500 + Rand() % 500;
    int T = 0;
    for (int i = 0; i < ExpireIn;) {
        PlotPixel(Data.Color, Data.X, Data.Y);

        int Delay = 16 + (T != 0);
        PerformDelay(Delay);
        i += Delay;
        T++;
        if (T == 3) {
            T = 0;
        }

        PlotPixel(BACKGROUND_COLOR, Data.X, Data.Y);

        // Update the particle
        Data.ActX += Data.VelX * DELTA_SEC;
        Data.ActY += Data.VelY * DELTA_SEC;

        Data.X = FP_TO_INT(Data.ActX);
        Data.Y = FP_TO_INT(Data.ActY);

        // Gravity
        Data.VelY += INT_TO_FP(10) * DELTA_SEC;
    }

    // Explode it!
    // This spawns many, many threads! Cause why not, right?!
    int PartCount = Rand() % 50 + 50;

    for (int i = 0; i < PartCount; i++) {
        FireworkData *DataClone = MmAllocatePool(sizeof(FireworkData), "FWKS");
        if (!DataClone) {
            break;
        }

        *DataClone = Data;
        SpawnParticle(DataClone);
    }

    PsTerminateThread();
}

static void SpawnParticle(FireworkData *Data) {
    PsThread *Thread = PsCreateThread(T_Particle, Data);
    if (Thread) {
        PsReadyThread(Thread);
    } else {
        MmFreePool(Data, "FWKS");
    }
}

static void SpawnExplodable(void) {
    PsThread *Thread = PsCreateThread(T_Explodable, NULL);
    if (Thread) {
        PsReadyThread(Thread);
    }
}

[[noreturn]] static void PerformFireworkTest(void) {
    VidSetColor(BACKGROUND_COLOR, 0xFFFFFF);
    VidResetDisplay();

    // The main thread occupies itself with spawning explodeables
    // from time to time, to keep things interesting.
    while (1) {
        int SpawnCount = Rand() % 2 + 1;

        for (int i = 0; i < SpawnCount; i++) {
            SpawnExplodable();
        }

        PerformDelay(2000 + Rand() % 2000);
    }
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function is the kernel's architecture-independent entry point.
 *     The next phase of boot is done after the process manager creates the system thread.
 *
 * PARAMETERS:
 *     LoaderBlockPage - Physical address of the OSLOADER's boot block.
 *
 * RETURN VALUE:
 *     Does not return.
 *-----------------------------------------------------------------------------------------------*/
[[noreturn]] void KiSystemStartup(uint64_t LoaderBlockPage) {
    KiLoaderBlock *LoaderBlock = MI_PADDR_TO_VADDR(LoaderBlockPage);
    KeProcessor *Processor = MI_PADDR_TO_VADDR((uint64_t)LoaderBlock->BootProcessor);

    /* Stage 1: Memory manager initialization; This won't mark the OSLOADER pages as free
     * just yet. */
    MiInitializePool(LoaderBlock);
    MiInitializePageAllocator(LoaderBlock);

    /* Stage 2: Display initialization (for early debugging). */
    VidpInitialize(LoaderBlock);
    VidPrint(
        VID_MESSAGE_INFO,
        "Kernel",
        "palladium kernel for %s, git commit %s\n",
        KE_ARCH,
        KE_GIT_HASH);

    /* Stage 3.1: Save all remaining info from the boot loader, and release the OSLOADER memory for
     * usage. */
    KiSaveAcpiData(LoaderBlock);
    KiSaveBootStartDrivers(LoaderBlock);
    MiReleaseOsloaderMemory(LoaderBlock);

    /* Stage 3.2: Early platform/arch initialization. */
    HalpInitializePlatform(Processor, 1);
    VidPrint(VID_MESSAGE_INFO, "Kernel", "%u processors online\n", HalpProcessorCount);

    /* Stage 4: Create the initial system thread, idle thread, and spin up the scheduler. */
    PspCreateSystemThread();
    PspCreateIdleThread();
    PspInitializeScheduler(1);

    while (1) {
        HalpStopProcessor();
    }
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function is the post-scheduler entry point; We're responsible for finishing the boot
 *     process.
 *
 * PARAMETERS:
 *     LoaderData - Where bootmgr placed our boot data.
 *
 * RETURN VALUE:
 *     Does not return.
 *-----------------------------------------------------------------------------------------------*/
[[noreturn]] void KiContinueSystemStartup(void *) {
    /* Stage 5: Initialize all boot drivers; We can't load anything further than this without
       them. */
    //KiRunBootStartDrivers();
    PerformFireworkTest();
}
