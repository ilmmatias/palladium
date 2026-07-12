/* SPDX-FileCopyrightText: (C) 2026 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <kernel/ev.h>
#include <kernel/halp.h>
#include <kernel/kd.h>
#include <kernel/ke.h>
#include <kernel/mi.h>
#include <kernel/ob.h>
#include <kernel/ps.h>
#include <rt/bitmap.h>
#include <rt/except.h>
#include <rt/hash.h>
#include <stdint.h>
#include <string.h>

#define KI_SELF_TEST_SEED UINT32_C(0xC001D00D)

typedef struct {
    EvMutex *Mutex;
    bool Acquired;
} MutexTimeoutContext;

static uint32_t ObjectDeleteCount;

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function records destruction of a fake object used by the object-manager tests.
 *
 * PARAMETERS:
 *     Object - Fake object body.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
static void DeleteTestObject(void *) {
    ObjectDeleteCount++;
}

static ObType TestObjectType = {
    .Name = "M1TestObject",
    .Size = sizeof(uint64_t),
    .Delete = DeleteTestObject,
};

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function reports a failed self-test and stops the test kernel.
 *
 * PARAMETERS:
 *     Suite - Name of the failed test suite.
 *     Case - Stable identifier of the failed test case.
 *     Code - Stable failure reason.
 *
 * RETURN VALUE:
 *     Does not return.
 *-----------------------------------------------------------------------------------------------*/
[[noreturn]] static void FailSelfTest(const char *Suite, uint32_t Case, uint32_t Code) {
    KdPrint(KD_TYPE_NONE, "M1TEST FAIL suite=%s case=%u code=%u\n", Suite, Case, Code);
    KeFatalError(KE_PANIC_MANUALLY_INITIATED_CRASH, Case, Code, 0, 0);
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function validates portable RT behavior using the target-linked implementation.
 *
 * PARAMETERS:
 *     None.
 *
 * RETURN VALUE:
 *     Number of completed test cases.
 *-----------------------------------------------------------------------------------------------*/
static uint32_t TestPortableRt(void) {
    static const uint8_t UnalignedData[] = {
        0xFF,
        '1',
        '2',
        '3',
        '4',
        '5',
        '6',
        '7',
        '8',
        '9',
        '0',
        '1',
        '2',
        '3',
        '4',
        '5',
        '6',
    };
    uint64_t Buffer[3] = {0};
    RtBitmap Bitmap;

    RtInitializeBitmap(&Bitmap, Buffer, 130);
    RtSetAllBits(&Bitmap);
    RtClearBits(&Bitmap, 120, 10);
    if (RtFindClearBits(&Bitmap, 120, 10) != 120) {
        FailSelfTest("portable-rt", 1, 1);
    }

    if (RtFindClearBits(&Bitmap, 121, 10) != UINT64_MAX) {
        FailSelfTest("portable-rt", 2, 1);
    }

    if (RtGetHash(UnalignedData + 1, 16) != UINT32_C(0x03BF5152)) {
        FailSelfTest("portable-rt", 3, 1);
    }

    return 3;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function initializes a synthetic amd64 unwind record used by the target-only tests.
 *
 * PARAMETERS:
 *     UnwindInfo - Storage for the unwind record.
 *     Version - Unwind format version.
 *     CountOfCodes - Number of initialized unwind-code slots.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
static void InitializeUnwindInfo(RtUnwindInfo *UnwindInfo, uint8_t Version, uint8_t CountOfCodes) {
    memset(UnwindInfo, 0, sizeof(RtUnwindInfo) + sizeof(RtUnwindCode) * 4);
    UnwindInfo->Version = Version;
    UnwindInfo->SizeOfProlog = 4;
    UnwindInfo->CountOfCodes = CountOfCodes;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function validates amd64 unwind version handling and version-2 epilog descriptors.
 *
 * PARAMETERS:
 *     None.
 *
 * RETURN VALUE:
 *     Number of completed test cases.
 *-----------------------------------------------------------------------------------------------*/
static uint32_t TestUnwind(void) {
    _Alignas(16) uint8_t Image[512] = {0};
    _Alignas(16) uint8_t UnwindStorage[sizeof(RtUnwindInfo) + sizeof(RtUnwindCode) * 4];
    RtUnwindInfo *UnwindInfo = (RtUnwindInfo *)UnwindStorage;
    RtRuntimeFunction FunctionEntry = {
        .BeginAddress = 0x40,
        .EndAddress = 0x60,
        .UnwindData = 0x100,
    };
    uint64_t Stack[8] = {0};
    RtContext Context = {0};
    uint64_t ReturnAddress = UINT64_C(0x1122334455667788);

    /* ADD RSP, 32; RET. */
    static const uint8_t Epilog[] = {0x48, 0x83, 0xC4, 0x20, 0xC3};
    memcpy(Image + 0x5B, Epilog, sizeof(Epilog));
    Stack[4] = ReturnAddress;

    InitializeUnwindInfo(UnwindInfo, 2, 2);
    UnwindInfo->UnwindCode[0].CodeOffset = sizeof(Epilog);
    UnwindInfo->UnwindCode[0].UnwindOp = RT_UWOP_EPILOG;
    UnwindInfo->UnwindCode[0].OpInfo = 1;
    UnwindInfo->UnwindCode[1].CodeOffset = 4;
    UnwindInfo->UnwindCode[1].UnwindOp = RT_UWOP_ALLOC_SMALL;
    UnwindInfo->UnwindCode[1].OpInfo = 3;
    memcpy(Image + FunctionEntry.UnwindData, UnwindInfo, sizeof(UnwindStorage));

    Context.Rsp = (uint64_t)&Stack[0];
    RtVirtualUnwind(
        RT_UNW_FLAG_NHANDLER,
        (uint64_t)Image,
        (uint64_t)(Image + 0x5B),
        &FunctionEntry,
        &Context,
        NULL,
        NULL);
    if (Context.Rip != ReturnAddress || Context.Rsp != (uint64_t)&Stack[5]) {
        FailSelfTest("amd64-unwind", 1, 1);
    }

    Context = (RtContext){0};
    Context.Rsp = (uint64_t)&Stack[0];
    RtVirtualUnwind(
        RT_UNW_FLAG_NHANDLER,
        (uint64_t)Image,
        (uint64_t)(Image + 0x50),
        &FunctionEntry,
        &Context,
        NULL,
        NULL);
    if (Context.Rip != ReturnAddress) {
        KdPrint(
            KD_TYPE_NONE,
            "M1TEST DIAGNOSTIC suite=amd64-unwind case=2 rip=%016llx expected=%016llx "
            "rsp=%016llx expected-rsp=%016llx\n",
            Context.Rip,
            ReturnAddress,
            Context.Rsp,
            (uint64_t)&Stack[5]);
        FailSelfTest("amd64-unwind", 2, 2);
    }
    if (Context.Rsp != (uint64_t)&Stack[5]) {
        FailSelfTest("amd64-unwind", 2, 3);
    }

    /* Describe an epilog which does not end at the function boundary. */
    FunctionEntry.EndAddress = 0x70;
    memcpy(Image + 0x60, Epilog, sizeof(Epilog));
    InitializeUnwindInfo(UnwindInfo, 2, 3);
    UnwindInfo->UnwindCode[0].CodeOffset = sizeof(Epilog);
    UnwindInfo->UnwindCode[0].UnwindOp = RT_UWOP_EPILOG;
    UnwindInfo->UnwindCode[1].CodeOffset = 0x10;
    UnwindInfo->UnwindCode[1].UnwindOp = RT_UWOP_EPILOG;
    UnwindInfo->UnwindCode[2].CodeOffset = 4;
    UnwindInfo->UnwindCode[2].UnwindOp = RT_UWOP_ALLOC_SMALL;
    UnwindInfo->UnwindCode[2].OpInfo = 3;
    memcpy(Image + FunctionEntry.UnwindData, UnwindInfo, sizeof(UnwindStorage));
    Context = (RtContext){0};
    Context.Rsp = (uint64_t)&Stack[0];
    RtVirtualUnwind(
        RT_UNW_FLAG_NHANDLER,
        (uint64_t)Image,
        (uint64_t)(Image + 0x60),
        &FunctionEntry,
        &Context,
        NULL,
        NULL);
    if (Context.Rip != ReturnAddress || Context.Rsp != (uint64_t)&Stack[5]) {
        FailSelfTest("amd64-unwind", 3, 1);
    }

    /* Unknown versions and truncated multi-slot opcodes leave the context untouched. */
    RtContext OriginalContext = {0};
    OriginalContext.Rsp = (uint64_t)&Stack[0];
    Context = OriginalContext;
    InitializeUnwindInfo(UnwindInfo, 3, 0);
    memcpy(Image + FunctionEntry.UnwindData, UnwindInfo, sizeof(UnwindStorage));
    RtVirtualUnwind(
        RT_UNW_FLAG_NHANDLER,
        (uint64_t)Image,
        (uint64_t)(Image + 0x50),
        &FunctionEntry,
        &Context,
        NULL,
        NULL);
    if (memcmp(&Context, &OriginalContext, sizeof(Context))) {
        FailSelfTest("amd64-unwind", 4, 1);
    }

    Context = OriginalContext;
    InitializeUnwindInfo(UnwindInfo, 1, 1);
    UnwindInfo->UnwindCode[0].CodeOffset = 4;
    UnwindInfo->UnwindCode[0].UnwindOp = RT_UWOP_ALLOC_LARGE;
    memcpy(Image + FunctionEntry.UnwindData, UnwindInfo, sizeof(UnwindStorage));
    RtVirtualUnwind(
        RT_UNW_FLAG_NHANDLER,
        (uint64_t)Image,
        (uint64_t)(Image + 0x50),
        &FunctionEntry,
        &Context,
        NULL,
        NULL);
    if (memcmp(&Context, &OriginalContext, sizeof(Context))) {
        FailSelfTest("amd64-unwind", 5, 1);
    }

    Context = OriginalContext;
    InitializeUnwindInfo(UnwindInfo, 1, 1);
    UnwindInfo->UnwindCode[0].CodeOffset = 4;
    UnwindInfo->UnwindCode[0].UnwindOp = RT_UWOP_SPARE_CODE;
    memcpy(Image + FunctionEntry.UnwindData, UnwindInfo, sizeof(UnwindStorage));
    RtVirtualUnwind(
        RT_UNW_FLAG_NHANDLER,
        (uint64_t)Image,
        (uint64_t)(Image + 0x50),
        &FunctionEntry,
        &Context,
        NULL,
        NULL);
    if (memcmp(&Context, &OriginalContext, sizeof(Context))) {
        FailSelfTest("amd64-unwind", 6, 1);
    }

    InitializeUnwindInfo(UnwindInfo, 2, 1);
    UnwindInfo->UnwindCode[0].CodeOffset = 4;
    UnwindInfo->UnwindCode[0].UnwindOp = RT_UWOP_ALLOC_SMALL;
    UnwindInfo->UnwindCode[0].OpInfo = 3;
    memcpy(Image + FunctionEntry.UnwindData, UnwindInfo, sizeof(UnwindStorage));
    Context = (RtContext){0};
    Context.Rsp = (uint64_t)&Stack[0];
    RtVirtualUnwind(
        RT_UNW_FLAG_NHANDLER,
        (uint64_t)Image,
        (uint64_t)(Image + 0x50),
        &FunctionEntry,
        &Context,
        NULL,
        NULL);
    if (Context.Rip != ReturnAddress || Context.Rsp != (uint64_t)&Stack[5]) {
        FailSelfTest("amd64-unwind", 7, 1);
    }

    return 7;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function validates transactional HAL mappings under deterministic page-table failure.
 *
 * PARAMETERS:
 *     None.
 *
 * RETURN VALUE:
 *     Number of completed test cases.
 *-----------------------------------------------------------------------------------------------*/
static uint32_t TestMappings(void) {
    const uint32_t ReservationPages = 1024;
    const uint64_t MappingPages = 513;
    void *Reservation = MiAllocatePoolSpace(ReservationPages);
    if (!Reservation) {
        FailSelfTest("mappings", 1, 1);
    }

    uint64_t Target = ((uint64_t)Reservation + HALP_PD_SIZE - 1) & ~(HALP_PD_SIZE - 1);
    uint64_t PtePages = MiTotalPtePages;
    HalpSetPageTableAllocationFailure(2);
    if (HalpMapContiguousPages(
            (void *)Target, MM_PAGE_SIZE, MappingPages << MM_PAGE_SHIFT, MI_MAP_WRITE)) {
        FailSelfTest("mappings", 2, 1);
    }
    if (HalpGetPageTableAllocationCount() != 2) {
        FailSelfTest("mappings", 2, 2);
    }
    HalpSetPageTableAllocationFailure(0);

    for (uint64_t Page = 0; Page < MappingPages; Page++) {
        if (HalpGetPhysicalAddress((void *)(Target + (Page << MM_PAGE_SHIFT)))) {
            FailSelfTest("mappings", 3, Page);
        }
    }
    if (MiTotalPtePages != PtePages) {
        FailSelfTest("mappings", 4, 1);
    }
    MiFreePoolSpace(Reservation, ReservationPages);
    if (!MiVerifyPageAccounting()) {
        FailSelfTest("mappings", 4, 2);
    }

    Reservation = MiAllocatePoolSpace(ReservationPages);
    if (!Reservation) {
        FailSelfTest("mappings", 5, 1);
    }
    Target = ((uint64_t)Reservation + HALP_PD_SIZE - 1) & ~(HALP_PD_SIZE - 1);
    uint64_t PhysicalAddresses[MappingPages];
    for (uint64_t Page = 0; Page < MappingPages; Page++) {
        PhysicalAddresses[Page] = (Page + 1) << MM_PAGE_SHIFT;
    }

    PtePages = MiTotalPtePages;
    HalpSetPageTableAllocationFailure(2);
    if (HalpMapNonContiguousPages(
            (void *)Target, PhysicalAddresses, MappingPages << MM_PAGE_SHIFT, MI_MAP_WRITE)) {
        FailSelfTest("mappings", 6, 1);
    }
    if (HalpGetPageTableAllocationCount() != 2) {
        FailSelfTest("mappings", 6, 2);
    }
    HalpSetPageTableAllocationFailure(0);
    for (uint64_t Page = 0; Page < MappingPages; Page++) {
        if (HalpGetPhysicalAddress((void *)(Target + (Page << MM_PAGE_SHIFT)))) {
            FailSelfTest("mappings", 7, Page);
        }
    }
    if (MiTotalPtePages != PtePages) {
        FailSelfTest("mappings", 7, UINT32_MAX);
    }
    MiFreePoolSpace(Reservation, ReservationPages);
    if (!MiVerifyPageAccounting()) {
        FailSelfTest("mappings", 7, UINT32_MAX - 1);
    }

    Reservation = MiAllocatePoolSpace(1);
    if (!Reservation ||
        !HalpMapContiguousPages(Reservation, MM_PAGE_SIZE, MM_PAGE_SIZE, MI_MAP_WRITE)) {
        FailSelfTest("mappings", 8, 1);
    }
    if (HalpMapContiguousPages(Reservation, 2 * MM_PAGE_SIZE, MM_PAGE_SIZE, MI_MAP_WRITE) ||
        HalpGetPhysicalAddress(Reservation) != MM_PAGE_SIZE) {
        FailSelfTest("mappings", 9, 1);
    }
    HalpUnmapPages(Reservation, MM_PAGE_SIZE);
    MiFreePoolSpace(Reservation, 1);
    return 9;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function attempts a bounded mutex acquisition for the executive self-test suite.
 *
 * PARAMETERS:
 *     ContextPointer - Mutex timeout test context.
 *
 * RETURN VALUE:
 *     Does not return.
 *-----------------------------------------------------------------------------------------------*/
[[noreturn]] static void MutexTimeoutThread(void *ContextPointer) {
    MutexTimeoutContext *Context = ContextPointer;
    Context->Acquired = EvAcquireMutex(Context->Mutex, EV_MILLISECS);
    if (Context->Acquired) {
        EvReleaseMutex(Context->Mutex);
    }

    PsTerminateThread();
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function supplies a thread body for the one-shot suspended-thread resume test.
 *
 * PARAMETERS:
 *     Context - Unused.
 *
 * RETURN VALUE:
 *     Does not return.
 *-----------------------------------------------------------------------------------------------*/
[[noreturn]] static void ResumeThread(void *) {
    PsTerminateThread();
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function validates immediate waits, finite timeout completion, mutex timeout recovery,
 *     thread completion signaling, and one-shot suspended-thread resume.
 *
 * PARAMETERS:
 *     None.
 *
 * RETURN VALUE:
 *     Number of completed test cases.
 *-----------------------------------------------------------------------------------------------*/
static uint32_t TestExecutive(void) {
    EvSignal *Signal = EvCreateSignal();
    if (!Signal) {
        FailSelfTest("executive", 1, 1);
    }

    if (EvWaitForObject(Signal, 0)) {
        FailSelfTest("executive", 2, 1);
    }

    if (EvWaitForObject(Signal, EV_MILLISECS)) {
        FailSelfTest("executive", 3, 1);
    }
    ObDereferenceObject(Signal);

    EvMutex *Mutex = EvCreateMutex();
    if (!Mutex || !EvAcquireMutex(Mutex, 0)) {
        FailSelfTest("executive", 4, 1);
    }

    MutexTimeoutContext Context = {
        .Mutex = Mutex,
        .Acquired = false,
    };
    PsThread *Thread = PsCreateThread(PS_CREATE_THREAD_DEFAULT, MutexTimeoutThread, &Context);
    if (!Thread || !EvWaitForObject(Thread, EV_TIMEOUT_UNLIMITED)) {
        FailSelfTest("executive", 5, 1);
    }

    if (Context.Acquired) {
        FailSelfTest("executive", 6, 1);
    }

    ObDereferenceObject(Thread);
    EvReleaseMutex(Mutex);
    if (!EvTryAcquireMutex(Mutex)) {
        FailSelfTest("executive", 7, 1);
    }
    EvReleaseMutex(Mutex);
    ObDereferenceObject(Mutex);

    Thread = PsCreateThread(PS_CREATE_THREAD_SUSPENDED, ResumeThread, NULL);
    if (!Thread || !PsResumeThread(Thread) || PsResumeThread(Thread)) {
        FailSelfTest("executive", 8, 1);
    }

    if (!EvWaitForObject(Thread, EV_TIMEOUT_UNLIMITED)) {
        FailSelfTest("executive", 9, 1);
    }
    ObDereferenceObject(Thread);
    return 9;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function validates referenced directory lookup, bounded name copying, duplicate and
 *     cycle rejection, explicit-parent removal, and destruction outside the directory locks.
 *
 * PARAMETERS:
 *     None.
 *
 * RETURN VALUE:
 *     Number of completed test cases.
 *-----------------------------------------------------------------------------------------------*/
static uint32_t TestObjects(void) {
    ObDirectory *Directory = ObCreateDirectory();
    void *FirstObject = ObCreateObject(&TestObjectType, MM_POOL_TAG_OBJECT);
    void *SecondObject = ObCreateObject(&TestObjectType, MM_POOL_TAG_OBJECT);
    if (!Directory || !FirstObject || !SecondObject) {
        FailSelfTest("objects", 1, 1);
    }

    if (!ObInsertIntoDirectory(Directory, "item", FirstObject) ||
        ObInsertIntoDirectory(Directory, "item", SecondObject)) {
        FailSelfTest("objects", 2, 1);
    }

    void *LookupObject = ObLookupDirectoryEntryByName(Directory, "item");
    if (LookupObject != FirstObject) {
        FailSelfTest("objects", 3, 1);
    }
    ObDereferenceObject(LookupObject);

    char Name[5];
    size_t RequiredNameSize;
    LookupObject = (void *)UINTPTR_MAX;
    if (ObLookupDirectoryEntryByIndex(
            Directory, 0, &LookupObject, Name, sizeof(Name) - 1, &RequiredNameSize) !=
            OB_LOOKUP_STATUS_BUFFER_TOO_SMALL ||
        LookupObject || RequiredNameSize != sizeof(Name)) {
        FailSelfTest("objects", 4, 1);
    }

    if (ObLookupDirectoryEntryByIndex(
            Directory, 0, &LookupObject, Name, sizeof(Name), &RequiredNameSize) !=
            OB_LOOKUP_STATUS_SUCCESS ||
        LookupObject != FirstObject || strcmp(Name, "item")) {
        FailSelfTest("objects", 5, 1);
    }
    ObDereferenceObject(LookupObject);

    if (!ObRemoveFromDirectory(Directory, FirstObject) ||
        ObRemoveFromDirectory(Directory, FirstObject)) {
        FailSelfTest("objects", 6, 1);
    }

    ObDereferenceObject(FirstObject);
    ObDereferenceObject(SecondObject);
    ObDereferenceObject(Directory);

    ObDirectory *Parent = ObCreateDirectory();
    ObDirectory *Child = ObCreateDirectory();
    if (!Parent || !Child || !ObInsertIntoDirectory(Parent, "child", Child) ||
        ObInsertIntoDirectory(Child, "parent", Parent)) {
        FailSelfTest("objects", 7, 1);
    }

    if (!ObRemoveFromDirectory(Parent, Child)) {
        FailSelfTest("objects", 8, 1);
    }
    ObDereferenceObject(Child);
    ObDereferenceObject(Parent);

    Directory = ObCreateDirectory();
    FirstObject = ObCreateObject(&TestObjectType, MM_POOL_TAG_OBJECT);
    if (!Directory || !FirstObject || !ObInsertIntoDirectory(Directory, "owned", FirstObject)) {
        FailSelfTest("objects", 9, 1);
    }
    ObDereferenceObject(FirstObject);
    ObDereferenceObject(Directory);
    if (ObjectDeleteCount != 3) {
        FailSelfTest("objects", 10, ObjectDeleteCount);
    }

    return 10;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function runs the private M1 kernel self-test suites after boot drivers initialize.
 *
 * PARAMETERS:
 *     None.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void KiRunSelfTests(void) {
    uint32_t Cases = 0;

    KdPrint(
        KD_TYPE_NONE,
        "M1TEST START suite=portable-rt seed=%08x cpus=%u\n",
        KI_SELF_TEST_SEED,
        HalpOnlineProcessorCount);
    uint32_t SuiteCases = TestPortableRt();
    Cases += SuiteCases;
    KdPrint(KD_TYPE_NONE, "M1TEST PASS suite=portable-rt cases=%u\n", SuiteCases);

    KdPrint(
        KD_TYPE_NONE,
        "M1TEST START suite=memory-accounting seed=%08x cpus=%u\n",
        KI_SELF_TEST_SEED,
        HalpOnlineProcessorCount);
    if (!MiVerifyPageAccounting()) {
        FailSelfTest("memory-accounting", 1, 1);
    }

    KdPrint(
        KD_TYPE_NONE,
        "M1TEST DIAGNOSTIC suite=memory-accounting managed=%llu unmanaged=%llu reserved=%llu "
        "used=%llu cached=%llu free=%llu\n",
        MiTotalManagedPages,
        MiTotalUnmanagedPages,
        MiTotalReservedPages,
        MiTotalUsedPages,
        MiTotalCachedPages,
        MiTotalFreePages);
    Cases++;
    KdPrint(KD_TYPE_NONE, "M1TEST PASS suite=memory-accounting cases=1\n");

    KdPrint(
        KD_TYPE_NONE,
        "M1TEST START suite=mappings seed=%08x cpus=%u\n",
        KI_SELF_TEST_SEED,
        HalpOnlineProcessorCount);
    SuiteCases = TestMappings();
    Cases += SuiteCases;
    KdPrint(KD_TYPE_NONE, "M1TEST PASS suite=mappings cases=%u\n", SuiteCases);

    KdPrint(
        KD_TYPE_NONE,
        "M1TEST START suite=executive seed=%08x cpus=%u\n",
        KI_SELF_TEST_SEED,
        HalpOnlineProcessorCount);
    SuiteCases = TestExecutive();
    Cases += SuiteCases;
    KdPrint(KD_TYPE_NONE, "M1TEST PASS suite=executive cases=%u\n", SuiteCases);

    KdPrint(
        KD_TYPE_NONE,
        "M1TEST START suite=objects seed=%08x cpus=%u\n",
        KI_SELF_TEST_SEED,
        HalpOnlineProcessorCount);
    SuiteCases = TestObjects();
    Cases += SuiteCases;
    KdPrint(KD_TYPE_NONE, "M1TEST PASS suite=objects cases=%u\n", SuiteCases);

    KdPrint(
        KD_TYPE_NONE,
        "M1TEST START suite=amd64-unwind seed=%08x cpus=%u\n",
        KI_SELF_TEST_SEED,
        HalpOnlineProcessorCount);
    SuiteCases = TestUnwind();
    Cases += SuiteCases;
    KdPrint(KD_TYPE_NONE, "M1TEST PASS suite=amd64-unwind cases=%u\n", SuiteCases);
    KdPrint(KD_TYPE_NONE, "M1TEST COMPLETE cases=%u\n", Cases);
}
