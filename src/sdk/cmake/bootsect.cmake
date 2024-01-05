# SPDX-FileCopyrightText: (C) 2023 ilmmatias
# SPDX-License-Identifier: GPL-3.0-or-later

set(CMAKE_ASM16_COMPILER jwasm)
set(CMAKE_ASM16_FLAGS "-q -bin -W3 -WX -Zg -Zne -X")

function(add_bootsector target file)
    add_custom_command(
        OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${target}.com
        COMMAND ${CMAKE_ASM16_COMPILER} ${CMAKE_ASM16_FLAGS}
                -Fo${CMAKE_CURRENT_BINARY_DIR}/${target}.com ${CMAKE_CURRENT_SOURCE_DIR}/${file}
        DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/${file}
    )

    add_custom_target(${target} ALL DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/${target}.com)
endfunction()
