# SPDX-FileCopyrightText: (C) 2023 ilmmatias
# SPDX-License-Identifier: BSD-3-Clause

project(os/drivers/ntfs)

add_executable(ntfs "knostdlib"
    main.c)

target_link_libraries(ntfs kernel)
target_include_directories(ntfs PRIVATE include)
target_compile_options(ntfs PRIVATE $<$<COMPILE_LANGUAGE:C,CXX>:-ffreestanding>)
target_link_options(ntfs PRIVATE -Wl,--subsystem=native,--entry=DriverEntry)
set_target_properties(ntfs PROPERTIES SUFFIX ".sys")
