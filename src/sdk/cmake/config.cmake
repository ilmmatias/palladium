# SPDX-FileCopyrightText: (C) 2023-2025 ilmmatias
# SPDX-License-Identifier: GPL-3.0-or-later

if(NOT ARCH)
    message(WARNING "Target architecture (ARCH) is required. Defaulting to amd64.")
    set(ARCH amd64)
endif()
string(TOLOWER ${ARCH} ARCH)

# Validate the architecture, and map the values into valid clang targets (as we expect
# clang+llvm to be used).
set(TARGET_amd64 x86_64)
if(NOT TARGET_${ARCH})
    message(FATAL_ERROR "Invalid target architecture (ARCH). Valid options are: amd64.")
endif()

# Use the newest C standard available (C23 as the time of the latest update to this file).
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_C_STANDARD 23)

# Enable all the errors!
set(CMAKE_C_FLAGS "-Wall -Wextra -Werror")
set(CMAKE_RC_FLAGS "--target=${TARGET_${ARCH}}-w64-mingw32")

# For debug-enabled build, make sure both line and column data is included.
set(CMAKE_C_FLAGS_DEBUG "-Og -g -glldb -gcolumn-info")
set(CMAKE_C_FLAGS_RELEASE "-O2")
set(CMAKE_C_FLAGS_MINSIZEREL "-Os")
set(CMAKE_C_FLAGS_RELWITHDEBINFO "-O2 -g -glldb -gcolumn-info")

# No flags needs to be passed to when creating .def files (for now at least).
set(CMAKE_LINK_DEF_FILE_FLAG "")

# Limit LTO to full/normal release builds.
if(NOT (CMAKE_BUILD_TYPE STREQUAL "Debug" OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo"))
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION ON)
endif()

# No need for a cross-compiler with clang, but we do need to instruct it to ignore the host
# system libraries.
add_link_options(-nostdlib -nodefaultlibs)
