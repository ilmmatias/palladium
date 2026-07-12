/* SPDX-FileCopyrightText: (C) 2026 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <kernel/halp.h>
#include <kernel/ki.h>
#include <os/intrin.h>
#include <stddef.h>
#include <stdint.h>

static bool HalpSerialEnabled = false;
static uint16_t HalpSerialBase = 0;

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function waits a bounded amount of time for the diagnostic serial port to accept a new
 *     byte. The diagnostic sink is disabled if the device does not become ready.
 *
 * PARAMETERS:
 *     None.
 *
 * RETURN VALUE:
 *     true if the device is ready, false otherwise.
 *-----------------------------------------------------------------------------------------------*/
static bool WaitForTransmitReady(void) {
    for (uint32_t Attempt = 0; Attempt < HALP_SERIAL_TX_POLL_LIMIT; Attempt++) {
        if (ReadPortByte(HalpSerialBase + HALP_SERIAL_REGISTER_LINE_STATUS) &
            HALP_SERIAL_LINE_STATUS_TX_EMPTY) {
            return true;
        }

        PauseProcessor();
    }

    __atomic_store_n(&HalpSerialEnabled, false, __ATOMIC_RELEASE);
    return false;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function writes one byte to the diagnostic serial port if the device is ready.
 *
 * PARAMETERS:
 *     Byte - Value to write.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
static void WriteByte(uint8_t Byte) {
    if (__atomic_load_n(&HalpSerialEnabled, __ATOMIC_ACQUIRE) && WaitForTransmitReady()) {
        WritePortByte(HalpSerialBase + HALP_SERIAL_REGISTER_DATA, Byte);
    }
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function initializes the boot-configured diagnostic device. Only a polling PC-compatible
 *     16550 port is currently supported on amd64.
 *
 * PARAMETERS:
 *     LoaderBlock - Data prepared by the boot loader.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void HalpInitializeDiagnosticDevice(KiLoaderBlock *LoaderBlock) {
    KiLoaderDiagnosticData *Diagnostic = &LoaderBlock->Diagnostic;
    if (Diagnostic->Type != KI_LOADER_DIAGNOSTIC_PC16550_PIO ||
        Diagnostic->Address > UINT16_MAX - 7 || !Diagnostic->BaudRate ||
        115200 % Diagnostic->BaudRate) {
        return;
    }

    uint32_t Divisor = 115200 / Diagnostic->BaudRate;
    if (!Divisor || Divisor > UINT16_MAX) {
        return;
    }

    HalpSerialBase = Diagnostic->Address;
    WritePortByte(HalpSerialBase + HALP_SERIAL_REGISTER_INTERRUPT_ENABLE, 0);
    WritePortByte(
        HalpSerialBase + HALP_SERIAL_REGISTER_LINE_CONTROL, HALP_SERIAL_LINE_CONTROL_DLAB);
    WritePortByte(HalpSerialBase + HALP_SERIAL_REGISTER_DATA, Divisor & UINT8_MAX);
    WritePortByte(HalpSerialBase + HALP_SERIAL_REGISTER_INTERRUPT_ENABLE, Divisor >> 8);
    WritePortByte(HalpSerialBase + HALP_SERIAL_REGISTER_LINE_CONTROL, HALP_SERIAL_LINE_CONTROL_8N1);
    WritePortByte(
        HalpSerialBase + HALP_SERIAL_REGISTER_FIFO_CONTROL, HALP_SERIAL_FIFO_ENABLE_CLEAR);
    WritePortByte(HalpSerialBase + HALP_SERIAL_REGISTER_MODEM_CONTROL, HALP_SERIAL_MODEM_DTR_RTS);
    __atomic_store_n(&HalpSerialEnabled, true, __ATOMIC_RELEASE);
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function checks whether the diagnostic serial device is available for output.
 *
 * PARAMETERS:
 *     None.
 *
 * RETURN VALUE:
 *     true if diagnostic output is enabled, false otherwise.
 *-----------------------------------------------------------------------------------------------*/
bool HalpDiagnosticDeviceEnabled(void) {
    return __atomic_load_n(&HalpSerialEnabled, __ATOMIC_ACQUIRE);
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function writes a character buffer to the diagnostic serial device. Newlines are
 *     expanded into the carriage-return/newline sequence expected by serial terminals.
 *
 * PARAMETERS:
 *     Buffer - Character data to write.
 *     Size - Size of the character data in bytes.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void HalpWriteDiagnosticDevice(const char *Buffer, size_t Size) {
    for (size_t i = 0; __atomic_load_n(&HalpSerialEnabled, __ATOMIC_ACQUIRE) && i < Size; i++) {
        if (Buffer[i] == '\n') {
            WriteByte('\r');
        }

        WriteByte(Buffer[i]);
    }
}
