# SPDX-FileCopyrightText: (C) 2023 ilmmatias
# SPDX-License-Identifier: GPL-3.0-or-later

project(os/boot/bootsect)

foreach(TARGET exfat fat32 iso9660 mbr ntfs)
    add_bootsector(${TARGET}boot ${TARGET}.asm)
endforeach()

add_bootsector(startup startup.asm)
