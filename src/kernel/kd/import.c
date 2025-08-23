/* SPDX-FileCopyrightText: (C) 2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <kernel/ev.h>
#include <kernel/halp.h>
#include <kernel/kdp.h>
#include <os/intrin.h>
#include <string.h>

extern KdpExtensibilityExports KdpDebugExports;

static bool BlockRecursion = false;

KdpExtensibilityImports KdpDebugImports = {};
uint32_t KdpDebugErrorStatus = 0;
uint16_t *KdpDebugErrorString = NULL;
uint32_t KdpDebugHardwareId = 0;

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function attempts to read data from the PCI configuration space.
 *
 * PARAMETERS:
 *     BusNumber - Just the bus number.
 *     SlotNumber - A combination of the device and function numbers.
 *     Buffer - Where to store the read data.
 *     Offset - Register offset inside the device config space.
 *     Length - How many bytes to read.
 *
 * RETURN VALUE:
 *     How many bytes we read.
 *-----------------------------------------------------------------------------------------------*/
static uint32_t GetPciDataByOffset(
    uint32_t BusNumber,
    uint32_t SlotNumber,
    void *Buffer,
    uint32_t Offset,
    uint32_t Length) {
    HalReadPciConfigurationSpace(
        BusNumber, SlotNumber & 0x1F, (SlotNumber >> 5) & 0x07, Offset, Buffer, Length);
    return Length;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function attempts to write data to the PCI configuration space.
 *
 * PARAMETERS:
 *     BusNumber - Just the bus number.
 *     SlotNumber - A combination of the device and function numbers.
 *     Buffer - Where to grab the data we should write.
 *     Offset - Register offset inside the device config space.
 *     Length - How many bytes to write.
 *
 * RETURN VALUE:
 *     How many bytes we wrote.
 *-----------------------------------------------------------------------------------------------*/
static uint32_t SetPciDataByOffset(
    uint32_t BusNumber,
    uint32_t SlotNumber,
    void *Buffer,
    uint32_t Offset,
    uint32_t Length) {
    HalWritePciConfigurationSpace(
        BusNumber, SlotNumber & 0x1F, (SlotNumber >> 5) & 0x07, Offset, Buffer, Length);
    return Length;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function attempts to extract the physical address of a given location.
 *
 * PARAMETERS:
 *     Va - Address we need the physical page of.
 *
 * RETURN VALUE:
 *     Physical address (directly written to .QuadPart), or 0 on failure (also written to
 *    .QuadPart).
 *-----------------------------------------------------------------------------------------------*/
static KdpPhysicalAddress GetPhysicalAddress(void *Va) {
    return (KdpPhysicalAddress){.QuadPart = HalpGetPhysicalAddress(Va)};
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function stops execution for a specified duration.
 *
 * PARAMETERS:
 *     Microseconds - The duration to wait in microseconds.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
static void StallExecutionProcessor(uint32_t Microseconds) {
    HalWaitTimer(Microseconds * EV_MICROSECS);
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function attempts to read a byte from the specified hardware register.
 *
 * PARAMETERS:
 *     Register - Address of the register.
 *
 * RETURN VALUE:
 *     Whatever we read from the register.
 *-----------------------------------------------------------------------------------------------*/
static uint8_t ReadRegisterUChar(uint8_t *Register) {
    return *(volatile uint8_t *)Register;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function attempts to read a word from the specified hardware register.
 *
 * PARAMETERS:
 *     Register - Address of the register.
 *
 * RETURN VALUE:
 *     Whatever we read from the register.
 *-----------------------------------------------------------------------------------------------*/
static uint16_t ReadRegisterUShort(uint16_t *Register) {
    return *(volatile uint16_t *)Register;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function attempts to read a dword from the specified hardware register.
 *
 * PARAMETERS:
 *     Register - Address of the register.
 *
 * RETURN VALUE:
 *     Whatever we read from the register.
 *-----------------------------------------------------------------------------------------------*/
static uint32_t ReadRegisterULong(uint32_t *Register) {
    return *(volatile uint32_t *)Register;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function attempts to read a qword from the specified hardware register.
 *
 * PARAMETERS:
 *     Register - Address of the register.
 *
 * RETURN VALUE:
 *     Whatever we read from the register.
 *-----------------------------------------------------------------------------------------------*/
static uint64_t ReadRegisterULong64(uint64_t *Register) {
    return *(volatile uint64_t *)Register;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function attempts to write a byte to the specified hardware register.
 *
 * PARAMETERS:
 *     Register - Address of the register.
 *     Value - What we should write.
 *
 * RETURN VALUE:
 *      None.
 *-----------------------------------------------------------------------------------------------*/
static void WriteRegisterUChar(uint8_t *Register, uint8_t Value) {
    *(volatile uint8_t *)Register = Value;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function attempts to write a word to the specified hardware register.
 *
 * PARAMETERS:
 *     Register - Address of the register.
 *     Value - What we should write.
 *
 * RETURN VALUE:
 *      None.
 *-----------------------------------------------------------------------------------------------*/
static void WriteRegisterUShort(uint16_t *Register, uint16_t Value) {
    *(volatile uint16_t *)Register = Value;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function attempts to write a dword to the specified hardware register.
 *
 * PARAMETERS:
 *     Register - Address of the register.
 *     Value - What we should write.
 *
 * RETURN VALUE:
 *      None.
 *-----------------------------------------------------------------------------------------------*/
static void WriteRegisterULong(uint32_t *Register, uint32_t Value) {
    *(volatile uint32_t *)Register = Value;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function attempts to write a qword to the specified hardware register.
 *
 * PARAMETERS:
 *     Register - Address of the register.
 *     Value - What we should write.
 *
 * RETURN VALUE:
 *      None.
 *-----------------------------------------------------------------------------------------------*/
static void WriteRegisterULong64(uint64_t *Register, uint64_t Value) {
    *(volatile uint64_t *)Register = Value;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function attempts to read a byte from the specified hardware port.
 *
 * PARAMETERS:
 *     Port - Address of the port.
 *
 * RETURN VALUE:
 *     Whatever we read from the port.
 *-----------------------------------------------------------------------------------------------*/
static uint8_t ReadPortUChar(uint8_t *Port) {
    return ReadPortByte((uintptr_t)Port);
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function attempts to read a word from the specified hardware port.
 *
 * PARAMETERS:
 *     Port - Address of the port.
 *
 * RETURN VALUE:
 *     Whatever we read from the port.
 *-----------------------------------------------------------------------------------------------*/
static uint16_t ReadPortUShort(uint16_t *Port) {
    return ReadPortWord((uintptr_t)Port);
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function attempts to read a dword from the specified hardware port.
 *
 * PARAMETERS:
 *     Port - Address of the port.
 *
 * RETURN VALUE:
 *     Whatever we read from the port.
 *-----------------------------------------------------------------------------------------------*/
static uint32_t ReadPortULong(uint32_t *Port) {
    return ReadPortDWord((uintptr_t)Port);
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function attempts to read a qword from the specified hardware port.
 *
 * PARAMETERS:
 *     Port - Address of the port.
 *
 * RETURN VALUE:
 *     Whatever we read from the port.
 *-----------------------------------------------------------------------------------------------*/
static uint32_t ReadPortULong64(uint64_t *Port) {
    /* The prototype for this function was directly grabbed from offical sources (WDK), but why does
     * it return a uint32_t instead of a uint64_t? It doesn't seem like anyone calls this, so we
     * mark it as broken for now. */
    KdPrint(KD_TYPE_ERROR, "attempted to call broken function: ReadPortULong64(%p)\n", Port);
    return UINT32_MAX;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function attempts to write a byte to the specified hardware port.
 *
 * PARAMETERS:
 *     Port - Address of the port.
 *     Value - What we should write.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
static void WritePortUChar(uint8_t *Port, uint8_t Value) {
    WritePortByte((uintptr_t)Port, Value);
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function attempts to write a word to the specified hardware port.
 *
 * PARAMETERS:
 *     Port - Address of the port.
 *     Value - What we should write.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
static void WritePortUShort(uint16_t *Port, uint16_t Value) {
    WritePortWord((uintptr_t)Port, Value);
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function attempts to write a dword to the specified hardware port.
 *
 * PARAMETERS:
 *     Port - Address of the port.
 *     Value - What we should write.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
static void WritePortULong(uint32_t *Port, uint32_t Value) {
    WritePortDWord((uintptr_t)Port, Value);
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function attempts to write a qword to the specified hardware port.
 *
 * PARAMETERS:
 *     Port - Address of the port.
 *     Value - What we should write.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
static void WritePortULong64(uint32_t *Port, uint64_t Value) {
    /* The prototype for this function was directly grabbed from offical sources (WDK), but why does
     * it take a uint32_t pointer instead of a uint64_t pointer? It doesn't seem like anyone calls
     * this, so we mark it as broken for now. */
    KdPrint(
        KD_TYPE_ERROR,
        "attempted to call broken function: WritePortULong64(%p, %#llx)\n",
        Port,
        Value);
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function marks some driver code for hibernate/resume support.
 *
 * PARAMETERS:
 *      MemoryMap - I'm not too sure what this is for, the extensibility modules just set it to
 *                  NULL though.
 *      Flags - Some flags to control our behaviour
 *      Address - Start of the region we should mark.
 *      Length - Size of the region we should mark.
 *      Tag - 4-characters tag for the region.
 *
 * RETURN VALUE:
 *
 *-----------------------------------------------------------------------------------------------*/
static void
SetHiberRange(void *MemoryMap, uint32_t Flags, void *Address, uint32_t Length, uint32_t Tag) {
    KdPrint(
        KD_TYPE_ERROR,
        "attempted to call unsupported function: SetHiberRange(%p, %#x, %p, %#x, %#x)\n",
        MemoryMap,
        Flags,
        Address,
        Length,
        Tag);
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function halts the system after an unrecoverable error.
 *
 * PARAMETERS:
 *     BugCheckCode - Number of the error code/message to be shown.
 *     BugCheckParameter1 - Parameter to help with debugging the panic reason.
 *     BugCheckParameter2 - Parameter to help with debugging the panic reason.
 *     BugCheckParameter3 - Parameter to help with debugging the panic reason.
 *     BugCheckParameter4 - Parameter to help with debugging the panic reason.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
static void BugCheckEx(
    uint32_t BugCheckCode,
    uint32_t BugCheckParameter1,
    uint32_t BugCheckParameter2,
    uint32_t BugCheckParameter3,
    uint32_t BugCheckParameter4) {
    KdPrint(
        KD_TYPE_ERROR,
        "attempted to call unsupported function: BugCheckEx(%#x, %#x, %#x, %#x, %#x)\n",
        BugCheckCode,
        BugCheckParameter1,
        BugCheckParameter2,
        BugCheckParameter3,
        BugCheckParameter4);
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function maps some physical region into a new virtual memory.
 *
 * PARAMETERS:
 *     PhysicalAddress - Start of the region we should map.
 *     NumberPages - How many pages the region should have.
 *     FlushCurrentTlb - Set this to true to flush each page as we map it.
 *
 * RETURN VALUE:
 *     Either a pointer mapped to the physical region, or NULL on failure.
 *-----------------------------------------------------------------------------------------------*/
static void *
MapPhysicalMemory(KdpPhysicalAddress PhysicalAddress, uint32_t NumberPages, bool FlushCurrentTlb) {
    (void)FlushCurrentTlb;
    return HalpMapEarlyMemory(PhysicalAddress.QuadPart, NumberPages << MM_PAGE_SHIFT, MI_MAP_WRITE);
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function releases some virtual memory previously reserved by MapPhysicalMemory.
 *
 * PARAMETERS:
 *     VirtualAddress - What address MapPhysicalMemory gave out.
 *     NumberPages - How many pages the region has.
 *     FlushCurrentTlb - Set this to true to flush each page as we unmap it.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
static void UnmapVirtualAddress(void *VirtualAddress, uint32_t NumberPages, bool FlushCurrentTlb) {
    (void)FlushCurrentTlb;
    HalpUnmapEarlyMemory(VirtualAddress, NumberPages << MM_PAGE_SHIFT);
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function reads the system timer/cycle counter.
 *
 * PARAMETERS:
 *     Frequency - Point this to a variable if you want to acquire the frequency of the system
 *                 timer.
 *
 * RETURN VALUE:
 *     Current value of the system timer.
 *-----------------------------------------------------------------------------------------------*/
static uint64_t ReadCycleCounter(uint64_t *Frequency) {
    if (Frequency) {
        *Frequency = HalGetTimerFrequency();
    }

    return HalGetTimerTicks();
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function shows some debugging-related information for the device driver.
 *
 * PARAMETERS:
 *     Format - Format string; Works the same as printf().
 *     ... - Variadic arguments.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
static void Printf(char *Format, ...) {
    /* KdPrint calls the debugger, so make sure to handle recursion properly! */
    if (BlockRecursion) {
        return;
    } else {
        BlockRecursion = true;
    }

    va_list Arguments;
    va_start(Arguments, Format);
    KdPrintVariadic(KD_TYPE_TRACE, Format, Arguments);
    va_end(Arguments);

    BlockRecursion = false;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function initializes the control path for the Hyper-V hypervisor (seemingly? I'm not
 *     too sure what this is actually supposed to do though).
 *
 * PARAMETERS:
 *     Context - Some private data for the hypervisor.
 *     Parameters - Pointer to the initialization parameters.
 *     UnreserveChannels - Set this to true/false if we should/shouldn't release previously
 *                         reserved channels.
 *     ArrivalRoutine - Callback when some vmbus stuff happens.
 *     ArrivalRoutineContext - Context for the callback above.
 *     RequestedVersion - Version of the vmbus protocol.
 *
 * RETURN VALUE:
 *     true/false depending on if we succeeded.
 *-----------------------------------------------------------------------------------------------*/
static bool VmbusInitialize(
    void *Context,
    void *Parameters,
    bool UnreserveChannels,
    void *ArrivalRoutine,
    void *ArrivalRoutineContext,
    uint32_t RequestedVersion) {
    KdPrint(
        KD_TYPE_ERROR,
        "attempted to call unsupported function: VmbusInitialize(%p, %p, %d, %p, %p, %#x)\n",
        Context,
        Parameters,
        UnreserveChannels,
        ArrivalRoutine,
        ArrivalRoutineContext,
        RequestedVersion);
    return false;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function attempts to get the vendor ID of the attached hypervisor.
 *
 * PARAMETERS:
 *     None.
 *
 * RETURN VALUE:
 *     Integer representing the vendor ID of the attached hypervisor, or 0 if a hypervisor isn't
 *     attached.
 *-----------------------------------------------------------------------------------------------*/
static uint64_t GetHypervisorVendorId(void) {
    KdPrint(KD_TYPE_ERROR, "attempted to call unsupported function: GetHypervisorVendorId()\n");
    return 0;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function does the initialization of the import table structure; The extensibility
 *     module needs this both to fill in the export table, and to communicate with the host OS.
 *
 * PARAMETERS:
 *     None.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void KdpInitializeImports(void) {
    memset(&KdpDebugImports, 0, sizeof(KdpExtensibilityImports));
    KdpDebugImports.FunctionCount = KDP_EXTENSIBILITY_IMPORT_COUNT;
    KdpDebugImports.Exports = &KdpDebugExports;
    KdpDebugImports.GetPciDataByOffset = GetPciDataByOffset;
    KdpDebugImports.SetPciDataByOffset = SetPciDataByOffset;
    KdpDebugImports.GetPhysicalAddress = GetPhysicalAddress;
    KdpDebugImports.StallExecutionProcessor = StallExecutionProcessor;
    KdpDebugImports.ReadRegisterUChar = ReadRegisterUChar;
    KdpDebugImports.ReadRegisterUShort = ReadRegisterUShort;
    KdpDebugImports.ReadRegisterULong = ReadRegisterULong;
    KdpDebugImports.ReadRegisterULong64 = ReadRegisterULong64;
    KdpDebugImports.WriteRegisterUChar = WriteRegisterUChar;
    KdpDebugImports.WriteRegisterUShort = WriteRegisterUShort;
    KdpDebugImports.WriteRegisterULong = WriteRegisterULong;
    KdpDebugImports.WriteRegisterULong64 = WriteRegisterULong64;
    KdpDebugImports.ReadPortUChar = ReadPortUChar;
    KdpDebugImports.ReadPortUShort = ReadPortUShort;
    KdpDebugImports.ReadPortULong = ReadPortULong;
    KdpDebugImports.ReadPortULong64 = ReadPortULong64;
    KdpDebugImports.WritePortUChar = WritePortUChar;
    KdpDebugImports.WritePortUShort = WritePortUShort;
    KdpDebugImports.WritePortULong = WritePortULong;
    KdpDebugImports.WritePortULong64 = WritePortULong64;
    KdpDebugImports.SetHiberRange = SetHiberRange;
    KdpDebugImports.BugCheckEx = BugCheckEx;
    KdpDebugImports.MapPhysicalMemory = MapPhysicalMemory;
    KdpDebugImports.UnmapVirtualAddress = UnmapVirtualAddress;
    KdpDebugImports.ReadCycleCounter = ReadCycleCounter;
    KdpDebugImports.Printf = Printf;
    KdpDebugImports.VmbusInitialize = VmbusInitialize;
    KdpDebugImports.GetHypervisorVendorId = GetHypervisorVendorId;
    KdpDebugImports.ExecutionEnvironment = KDP_ENVIRONMENT_KERNEL;
    KdpDebugImports.ErrorStatus = &KdpDebugErrorStatus;
    KdpDebugImports.ErrorString = &KdpDebugErrorString;
    KdpDebugImports.HardwareId = &KdpDebugHardwareId;
}
