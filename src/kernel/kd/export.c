/* SPDX-FileCopyrightText: (C) 2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <kernel/kdp.h>
#include <string.h>

KdpExtensibilityExports KdpDebugExports = {};

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function initializes the device controller for use by the debugger. After
 *     initialization, the controller is running at its highest support speed, on pooling mode
 *     (with interrupts masked).
 *
 * PARAMETERS:
 *     KdNet - Pointer containing useful data for the controller driver.
 *
 * RETURN VALUE:
 *     NTSTATUS values describing the result of the operation (anything but STATUS_SUCCESS is to
 *     be considered an error).
 *-----------------------------------------------------------------------------------------------*/
uint32_t KdpInitializeController(KdpExtensibilityData *KdNet) {
    return KdpDebugExports.InitializeController(KdNet);
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function notifies the driver that the device controller is no longer going to be used
 *     by the debugger.
 *
 * PARAMETERS:
 *     Adapter - Pointer containing the private device driver data.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void KdpShutdownController(void *Adapter) {
    KdpDebugExports.ShutdownController(Adapter);
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function marks the device controller driver code for hibernate/resume support.
 *
 * PARAMETERS:
 *     None.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void KdpSetHibernateRange(void) {
    KdpDebugExports.SetHibernateRange();
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function attempts to receive a packet from the device controller (which is assumed to be
 *     a network/network-like controller).
 *
 * PARAMETERS:
 *     Adapter - Pointer containing the private device driver data.
 *     Handle - Somewhere to store the handle for the received packet.
 *     Packet - Somewhere to store where the data of the received packet starts.
 *     Length - Somewhere to store the size of the received packet.
 *
 * RETURN VALUE:
 *     NTSTATUS values describing the result of the operation (anything but STATUS_SUCCESS is to
 *     be considered an error).
 *-----------------------------------------------------------------------------------------------*/
uint32_t KdpGetRxPacket(void *Adapter, uint32_t *Handle, void **Packet, uint32_t *Length) {
    return KdpDebugExports.GetRxPacket(Adapter, Handle, Packet, Length);
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function frees a previously received packet back for use by the device controller (which
 *     is assumed to be a network/network-like controller).
 *
 * PARAMETERS:
 *     Adapter - Pointer containing the private device driver data.
 *     Handle - Handle previously obtained from KdpGetRxPacket.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void KdpReleaseRxPacket(void *Adapter, uint32_t Handle) {
    KdpDebugExports.ReleaseRxPacket(Adapter, Handle);
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function attempts to grab a handle to send a new packet via the device controller (which
 *     is assumed to be a network/network-like packet).
 *
 * PARAMETERS:
 *     Adapter - Pointer containing the private device driver data.
 *     Handle - Somewhere to store the handle for the new packet.
 *
 * RETURN VALUE:
 *     NTSTATUS values describing the result of the operation (anything but STATUS_SUCCESS is to
 *     be considered an error).
 *-----------------------------------------------------------------------------------------------*/
uint32_t KdpGetTxPacket(void *Adapter, uint32_t *Handle) {
    return KdpDebugExports.GetTxPacket(Adapter, Handle);
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function attempts to send and free a previously allocated transmit packet.
 *
 * PARAMETERS:
 *     Adapter - Pointer containing the private device driver data.
 *     Handle - Handle previously obtained from KdpGetTxPacket.
 *     Length - How many bytes of the packet buffer to send.
 *
 * RETURN VALUE:
 *     NTSTATUS values describing the result of the operation (anything but STATUS_SUCCESS is to
 *     be considered an error).
 *-----------------------------------------------------------------------------------------------*/
uint32_t KdpSendTxPacket(void *Adapter, uint32_t Handle, uint32_t Length) {
    return KdpDebugExports.SendTxPacket(Adapter, Handle, Length);
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function gets the buffer address for a previously acquired packet handle.
 *
 * PARAMETERS:
 *     Adapter - Pointer containing the private device driver data.
 *     Handle - Handle previously obtained from KdpGetRx/TxPacket.
 *
 * RETURN VALUE:
 *     Pointer to the packet buffer, or NULL if the handle is invalid.
 *-----------------------------------------------------------------------------------------------*/
void *KdpGetPacketAddress(void *Adapter, uint32_t Handle) {
    return KdpDebugExports.GetPacketAddress(Adapter, Handle);
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function gets the length of a previously acquired packet handle.
 *
 * PARAMETERS:
 *     Adapter - Pointer containing the private device driver data.
 *     Handle - Handle previously obtained from KdpGetRx/TxPacket.
 *
 * RETURN VALUE:
 *     Length of the packet buffer (max size for transmit handles, actual received size for receive
 *     handles).
 *-----------------------------------------------------------------------------------------------*/
uint32_t KdpGetPacketLength(void *Adapter, uint32_t Handle) {
    return KdpDebugExports.GetPacketLength(Adapter, Handle);
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function obtains the wanted size for the device driver private data structure.
 *
 * PARAMETERS:
 *     Device - Previously initialized PCI device data.
 *
 * RETURN VALUE:
 *     Size of the structure in bytes.
 *-----------------------------------------------------------------------------------------------*/
uint32_t KdpGetHardwareContextSize(KdpDebugDeviceDescriptor *Device) {
    return KdpDebugExports.GetHardwareContextSize(Device);
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function sends a command to the device controller.
 *
 * PARAMETERS:
 *     Adapter - Pointer containing the private device driver data.
 *     RequestCode - Command code for the controller.
 *     InputBuffer - Buffer containing some data the controller can/should read.
 *     InputBufferLength - Size of the input buffer in bytes.
 *     OutputBuffer - Buffer where the controller can/should write its response.
 *     OutputBufferLength - Size of the output buffer in bytes.
 *
 * RETURN VALUE:
 *     NTSTATUS values describing the result of the operation (anything but STATUS_SUCCESS is to
 *     be considered an error).
 *-----------------------------------------------------------------------------------------------*/
uint32_t KdpDeviceControl(
    void *Adapter,
    uint32_t RequestCode,
    void *InputBuffer,
    uint32_t InputBufferLength,
    void *OutputBuffer,
    uint32_t OutputBufferLength) {
    return KdpDebugExports.DeviceControl(
        Adapter, RequestCode, InputBuffer, InputBufferLength, OutputBuffer, OutputBufferLength);
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function attempts to read the next byte from the serial port (if the debugger device is
 *     actually a serial-like device, rather than a network or similar controller).
 *
 * PARAMETERS:
 *     Adapter - Pointer containing the private serial driver data.
 *     Byte - Somewhere to store the read data.
 *
 * RETURN VALUE:
 *     NTSTATUS values describing the result of the operation (anything but STATUS_SUCCESS is to
 *     be considered an error).
 *-----------------------------------------------------------------------------------------------*/
uint32_t KdpReadSerialByte(void *Adapter, uint8_t *Byte) {
    return KdpDebugExports.ReadSerialByte(Adapter, Byte);
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function attempts to write a byte into the serial port (if the debugger device is
 *     actually a serial-like device, rather than a network or similar controller).
 *
 * PARAMETERS:
 *     Adapter - Pointer containing the private serial driver data.
 *     Byte - What data to try writing.
 *
 * RETURN VALUE:
 *     NTSTATUS values describing the result of the operation (anything but STATUS_SUCCESS is to
 *     be considered an error).
 *-----------------------------------------------------------------------------------------------*/
uint32_t KdpWriteSerialByte(void *Adapter, uint8_t Byte) {
    return KdpDebugExports.WriteSerialByte(Adapter, Byte);
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function attempts to initialize the device controller (if the debugger device is
 *     actually a serial-like device, rather than a network or similar controller).
 *
 * PARAMETERS:
 *     Device - Previously initialized PCI device data.
 *     Address - Physical address of the device.
 *
 * RETURN VALUE:
 *      NTSTATUS values describing the result of the operation (anything but STATUS_SUCCESS is to
 *      be considered an error).
 *-----------------------------------------------------------------------------------------------*/
uint32_t KdpSerialOutputInit(KdpDebugDeviceDescriptor *Device, KdpPhysicalAddress *Address) {
    return KdpDebugExports.SerialOutputInit(Device, Address);
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function attempts to write a byte into the serial port (if the debugger device is
 *     actually a serial-like device, rather than a network or similar controller).
 *
 * PARAMETERS:
 *     Byte - What data to try writing.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void KdpSerialOutputByte(uint8_t Byte) {
    KdpDebugExports.SerialOutputByte(Byte);
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function does some early initialization of the export table structure; The extensibility
 *     module still needs to fill in the function pointers before we can use it.
 *
 * PARAMETERS:
 *     None.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void KdpInitializeExports(void) {
    /* KdInitializeLibrary inside the extensibility module will fill up this structure, so we only
     * have to initialize the function count (which is used to check the host OS version). */
    memset(&KdpDebugExports, 0, sizeof(KdpExtensibilityExports));
    KdpDebugExports.FunctionCount = KDP_EXTENSIBILITY_EXPORT_COUNT;
}
