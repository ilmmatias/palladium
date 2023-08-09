# SPDX-FileCopyrightText: (C) 2023 ilmmatias
# SPDX-License-Identifier: BSD-3-Clause

project(os/boot/bootsect)

foreach(TARGET exfat fat32 iso9660 ntfs)
    add_bootsector(${TARGET}boot ${TARGET}.asm)
endforeach()
