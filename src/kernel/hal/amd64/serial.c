/* SPDX-FileCopyrightText: (C) 2026 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <kernel/halp.h>
#include <os/intrin.h>
#include <stdint.h>

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function initializes a polling PC-compatible 16550 serial port.
 *
 * PARAMETERS:
 *     Port - Serial-port state to initialize.
 *     Address - Base PIO address of the serial port.
 *     BaudRate - Requested baud rate using the conventional 115200-Hz divisor base.
 *
 * RETURN VALUE:
 *     true if the port configuration is valid, false otherwise.
 *-----------------------------------------------------------------------------------------------*/
bool HalpInitializeSerialPort(HalpSerialPort *Port, uint64_t Address, uint32_t BaudRate) {
    if (!Port || Address > UINT16_MAX - 7 || !BaudRate || 115200 % BaudRate) {
        return false;
    }

    uint32_t Divisor = 115200 / BaudRate;
    if (!Divisor || Divisor > UINT16_MAX) {
        return false;
    }

    Port->Base = Address;
    Port->Enabled = false;
    WritePortByte(Port->Base + HALP_SERIAL_REGISTER_INTERRUPT_ENABLE, 0);
    WritePortByte(Port->Base + HALP_SERIAL_REGISTER_LINE_CONTROL, HALP_SERIAL_LINE_CONTROL_DLAB);
    WritePortByte(Port->Base + HALP_SERIAL_REGISTER_DATA, Divisor & UINT8_MAX);
    WritePortByte(Port->Base + HALP_SERIAL_REGISTER_INTERRUPT_ENABLE, Divisor >> 8);
    WritePortByte(Port->Base + HALP_SERIAL_REGISTER_LINE_CONTROL, HALP_SERIAL_LINE_CONTROL_8N1);
    WritePortByte(Port->Base + HALP_SERIAL_REGISTER_FIFO_CONTROL, HALP_SERIAL_FIFO_ENABLE_CLEAR);
    WritePortByte(Port->Base + HALP_SERIAL_REGISTER_MODEM_CONTROL, HALP_SERIAL_MODEM_DTR_RTS);
    __atomic_store_n(&Port->Enabled, true, __ATOMIC_RELEASE);
    return true;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function reads one available byte without waiting.
 *
 * PARAMETERS:
 *     Port - Initialized serial-port state.
 *     Byte - Destination for the received byte.
 *
 * RETURN VALUE:
 *     true if a byte was available, false otherwise.
 *-----------------------------------------------------------------------------------------------*/
bool HalpReadSerialPort(HalpSerialPort *Port, uint8_t *Byte) {
    if (!Port || !Byte || !__atomic_load_n(&Port->Enabled, __ATOMIC_ACQUIRE) ||
        !(ReadPortByte(Port->Base + HALP_SERIAL_REGISTER_LINE_STATUS) &
          HALP_SERIAL_LINE_STATUS_RX_READY)) {
        return false;
    }

    *Byte = ReadPortByte(Port->Base + HALP_SERIAL_REGISTER_DATA);
    return true;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function writes one byte after a bounded transmit-ready wait.
 *
 * PARAMETERS:
 *     Port - Initialized serial-port state.
 *     Byte - Byte to transmit.
 *
 * RETURN VALUE:
 *     true if the byte was sent, false if the port became unavailable.
 *-----------------------------------------------------------------------------------------------*/
bool HalpWriteSerialPort(HalpSerialPort *Port, uint8_t Byte) {
    if (!Port || !__atomic_load_n(&Port->Enabled, __ATOMIC_ACQUIRE)) {
        return false;
    }

    for (uint32_t Attempt = 0; Attempt < HALP_SERIAL_TX_POLL_LIMIT; Attempt++) {
        if (ReadPortByte(Port->Base + HALP_SERIAL_REGISTER_LINE_STATUS) &
            HALP_SERIAL_LINE_STATUS_TX_EMPTY) {
            WritePortByte(Port->Base + HALP_SERIAL_REGISTER_DATA, Byte);
            return true;
        }

        PauseProcessor();
    }

    __atomic_store_n(&Port->Enabled, false, __ATOMIC_RELEASE);
    return false;
}
