# SPDX-FileCopyrightText: (C) 2023 yuuma03
# SPDX-License-Identifier: BSD-3-Clause

if(NOT ARCH)
    message(FATAL_ERROR "Target architecture (ARCH) is required. Valid options are: amd64, x86.")
endif()
string(TOLOWER ${ARCH} ARCH)

# Validate the architecture, and map the values into valid clang targets (as we expect clang+llvm to be used).
set(TARGET_amd64 x86_64)
set(TARGET_x86 i686)

if(NOT TARGET_${ARCH})
    message(FATAL_ERROR "Invalid target architecture (ARCH). Valid options are: amd64, x86.")
endif()

set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_C_STANDARD 23)
set(CMAKE_CXX_STANDARD 23)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -Werror")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Werror")

set(CMAKE_LINK_DEF_FILE_FLAG "")

add_link_options(-nostdlib -nodefaultlibs)
