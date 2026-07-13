/* SPDX-FileCopyrightText: (C) 2026 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <kernel/halp.h>
#include <kernel/kdp.h>
#include <kernel/ki.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#define KDP_SERIAL_RAW_CAPACITY (KDP_PROTOCOL_MAX_DATAGRAM + 6)
#define KDP_SERIAL_ENCODED_CAPACITY (KDP_SERIAL_RAW_CAPACITY + 8)

static HalpSerialPort Port;
static uint8_t EncodedBuffer[KDP_SERIAL_ENCODED_CAPACITY];
static size_t EncodedSize;

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function calculates the Castagnoli CRC used by serial debugger frames.
 *
 * PARAMETERS:
 *     Buffer - Data covered by the CRC.
 *     Size - Number of bytes in the buffer.
 *
 * RETURN VALUE:
 *     CRC32C value.
 *-----------------------------------------------------------------------------------------------*/
static uint32_t CalculateCrc32c(const uint8_t *Buffer, size_t Size) {
    uint32_t Crc = UINT32_MAX;
    for (size_t i = 0; i < Size; i++) {
        Crc ^= Buffer[i];
        for (int Bit = 0; Bit < 8; Bit++) {
            Crc = (Crc >> 1) ^ ((Crc & 1) ? 0x82F63B78 : 0);
        }
    }

    return Crc ^ UINT32_MAX;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function decodes one COBS frame into its unescaped body.
 *
 * PARAMETERS:
 *     Input - Encoded frame without the zero delimiter.
 *     InputSize - Encoded frame size.
 *     Output - Destination body.
 *     OutputSize - Returned body size.
 *
 * RETURN VALUE:
 *     true if the frame was well formed, false otherwise.
 *-----------------------------------------------------------------------------------------------*/
static bool
DecodeCobs(const uint8_t *Input, size_t InputSize, uint8_t *Output, size_t *OutputSize) {
    if (!InputSize) {
        return false;
    }

    size_t InputOffset = 0;
    size_t OutputOffset = 0;
    while (InputOffset < InputSize) {
        uint8_t Code = Input[InputOffset++];
        if (!Code || InputOffset + Code - 1 > InputSize ||
            OutputOffset + Code > KDP_SERIAL_RAW_CAPACITY + 1) {
            return false;
        }

        size_t CopySize = Code - 1;
        memcpy(Output + OutputOffset, Input + InputOffset, CopySize);
        OutputOffset += CopySize;
        InputOffset += CopySize;
        if (Code != UINT8_MAX && InputOffset < InputSize) {
            if (OutputOffset >= KDP_SERIAL_RAW_CAPACITY) {
                return false;
            }
            Output[OutputOffset++] = 0;
        }
    }

    *OutputSize = OutputOffset;
    return true;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function initializes the boot-configured serial debugger transport.
 *
 * PARAMETERS:
 *     LoaderBlock - Validated loader block.
 *
 * RETURN VALUE:
 *     true if the PC16550 port was initialized, false otherwise.
 *-----------------------------------------------------------------------------------------------*/
bool KdpInitializeSerialTransport(KiLoaderBlock *LoaderBlock) {
    EncodedSize = 0;
    return HalpInitializeSerialPort(
        &Port, LoaderBlock->Debug.Serial.Address, LoaderBlock->Debug.Serial.BaudRate);
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function polls and dispatches all bytes currently available from the serial port.
 *
 * PARAMETERS:
 *     State - Early-handshake or runtime debugger state.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void KdpPollSerialTransport(int State) {
    uint8_t Byte = 0;
    uint8_t RawBuffer[KDP_SERIAL_RAW_CAPACITY];
    while (HalpReadSerialPort(&Port, &Byte)) {
        if (Byte) {
            if (EncodedSize < sizeof(EncodedBuffer)) {
                EncodedBuffer[EncodedSize++] = Byte;
            }
            continue;
        }

        size_t RawSize = 0;
        bool Valid = DecodeCobs(EncodedBuffer, EncodedSize, RawBuffer, &RawSize);
        EncodedSize = 0;
        if (!Valid || RawSize < 6) {
            continue;
        }

        uint16_t DatagramSize = 0;
        uint32_t ExpectedCrc = 0;
        memcpy(&DatagramSize, RawBuffer, sizeof(DatagramSize));
        if (DatagramSize > KDP_PROTOCOL_MAX_DATAGRAM || RawSize != DatagramSize + 6) {
            continue;
        }
        memcpy(&ExpectedCrc, RawBuffer + 2 + DatagramSize, sizeof(ExpectedCrc));
        if (CalculateCrc32c(RawBuffer, 2 + DatagramSize) != ExpectedCrc) {
            continue;
        }

        uint8_t EmptyHardwareAddress[6] = {0};
        uint8_t EmptyProtocolAddress[4] = {0};
        KdpParseDebugPacket(
            State, EmptyHardwareAddress, EmptyProtocolAddress, 0, RawBuffer + 2, DatagramSize);
    }
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function frames and sends one protocol datagram over the serial debugger port.
 *
 * PARAMETERS:
 *     Buffer - Protocol datagram.
 *     Size - Datagram size.
 *
 * RETURN VALUE:
 *     true if the complete frame was sent, false otherwise.
 *-----------------------------------------------------------------------------------------------*/
bool KdpSendSerialDatagram(const void *Buffer, size_t Size) {
    if (!Buffer || Size > KDP_PROTOCOL_MAX_DATAGRAM) {
        return false;
    }

    uint8_t RawBuffer[KDP_SERIAL_RAW_CAPACITY];
    uint16_t DatagramSize = Size;
    memcpy(RawBuffer, &DatagramSize, sizeof(DatagramSize));
    memcpy(RawBuffer + 2, Buffer, Size);
    uint32_t Crc = CalculateCrc32c(RawBuffer, Size + 2);
    memcpy(RawBuffer + Size + 2, &Crc, sizeof(Crc));
    size_t RawSize = Size + 6;

    uint8_t Frame[KDP_SERIAL_ENCODED_CAPACITY];
    size_t CodeOffset = 0;
    size_t FrameSize = 1;
    uint8_t Code = 1;
    for (size_t i = 0; i < RawSize; i++) {
        if (!RawBuffer[i]) {
            Frame[CodeOffset] = Code;
            CodeOffset = FrameSize++;
            Code = 1;
        } else {
            Frame[FrameSize++] = RawBuffer[i];
            if (++Code == UINT8_MAX) {
                Frame[CodeOffset] = Code;
                CodeOffset = FrameSize++;
                Code = 1;
            }
        }
    }
    Frame[CodeOffset] = Code;
    Frame[FrameSize++] = 0;

    for (size_t i = 0; i < FrameSize; i++) {
        if (!HalpWriteSerialPort(&Port, Frame[i])) {
            return false;
        }
    }

    return true;
}
