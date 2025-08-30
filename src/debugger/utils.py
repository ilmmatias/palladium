# SPDX-FileCopyrightText: (C) 2025 ilmmatias
# SPDX-License-Identifier: GPL-3.0-or-later

import struct

from . import interface

#--------------------------------------------------------------------------------------------------
# PURPOSE:
#     This function handles the printing out any received memory data from the kernel.
#
# PARAMETERS:
#     Kind - Which memory type we're reading (physical vs virtual).
#     Payload - What memory data we should print.
#     Address - What address we read.
#     ItemSize - Size of each item to print.
#     Length - Total size in bytes of the region we're dumping.
#
# RETURN VALUE:
#     None.
#--------------------------------------------------------------------------------------------------
def KdpPrintMemoryData(
    Kind: str,
    Payload: bytes,
    Address: int,
    ItemSize: int,
    Length: int) -> None:
    # Map related to how we should parse the buffer items.
    FormatMap = {
        1: ("<B", "02x", 16),
        2: ("<H", "04x", 8),
        4: ("<L", "08x", 4),
        8: ("<Q", "016x", 2)
    }

    UnpackFormat, ItemFormat, ItemsPerLine = FormatMap[ItemSize]
    BytesPerLine = ItemsPerLine * ItemSize

    interface.KdPrint(
        interface.KD_DEST_COMMAND,
        interface.KD_TYPE_INFO,
        f"showing data contained in the {Kind} range " +
        f"0x{Address:x} - 0x{(Address + Length):x}\n")

    for i in range(0, len(Payload), BytesPerLine):
        # Unpack all items inside the current chunk (so we can later join them).
        Chunk = Payload[i:i + BytesPerLine]
        Items = []
        for j in range(0, len(Chunk), ItemSize):
            ItemBytes = Chunk[j:j + ItemSize]
            if len(ItemBytes) < ItemSize:
                break
            ItemValue: int = struct.unpack(UnpackFormat, ItemBytes)[0]
            Items.append(f"{ItemValue:{ItemFormat}}")

        AddressString = f"{(Address + i):016x}: "
        HexString = " ".join(Items)

        # And also show their ASCII representation (only signle-byte mode though).
        if ItemSize == 1:
            AsciiString = "".join(chr(Item) if 32 <= Item < 127 else "." for Item in Chunk)
            AsciiString = f" |{AsciiString}|\n"
        else:
            AsciiString = "\n"

        # And show everything + the current address.
        interface.KdPrint(
            interface.KD_DEST_COMMAND,
            interface.KD_TYPE_NONE,
            AddressString + HexString + AsciiString)
