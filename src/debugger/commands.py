# SPDX-FileCopyrightText: (C) 2025-2026 ilmmatias
# SPDX-License-Identifier: GPL-3.0-or-later

"""Pure debugger command parser."""

from dataclasses import dataclass
import re
import shlex
from typing import Union


class CommandError(ValueError):
    pass


@dataclass(frozen=True)
class MemoryCommand:
    physical: bool
    disassemble: bool
    address: int
    item_size: int
    count: int


@dataclass(frozen=True)
class PortCommand:
    address: int
    item_size: int


@dataclass(frozen=True)
class LocalCommand:
    name: str
    arguments: tuple[str, ...] = ()


Command = Union[MemoryCommand, PortCommand, LocalCommand]
MAX_MEMORY_PAYLOAD = 1006


def parse(line: str) -> Command | None:
    try:
        tokens = shlex.split(line)
    except ValueError as error:
        raise CommandError(str(error)) from error
    if not tokens:
        return None
    word = tokens[0].lower()
    if word in ("q", "quit", "h", "help"):
        if len(tokens) != 1:
            raise CommandError(f"{word} takes no arguments")
        return LocalCommand("quit" if word in ("q", "quit") else "help")
    if word == "el" or word.startswith("el/"):
        if len(tokens) != 2 or (word != "el" and word not in ("el/k", "el/c")):
            raise CommandError("expected format: el[/target] <path>")
        return LocalCommand("export", (word[3:] if word != "el" else "", tokens[1]))
    match = re.fullmatch(r"(dp|dv)(?:/([1-9][0-9]*))?", word)
    if match:
        if len(tokens) != 2:
            raise CommandError(f"expected format: {match[1]}[/count] <address>")
        count = int(match[2] or "128")
        if count > MAX_MEMORY_PAYLOAD:
            raise CommandError(f"read length cannot exceed {MAX_MEMORY_PAYLOAD} bytes")
        return MemoryCommand(match[1] == "dp", True, _address(tokens[1]), 1, count)
    match = re.fullmatch(r"(rp|rv)/([bwdq])([1-9][0-9]*)?", word)
    if match:
        if len(tokens) != 2:
            raise CommandError(f"expected format: {match[1]}/<size>[count] <address>")
        size = {"b": 1, "w": 2, "d": 4, "q": 8}[match[2]]
        count = int(match[3]) if match[3] else 128 // size
        if count > MAX_MEMORY_PAYLOAD // size:
            raise CommandError(f"read length cannot exceed {MAX_MEMORY_PAYLOAD} bytes")
        return MemoryCommand(match[1] == "rp", False, _address(tokens[1]), size, count)
    match = re.fullmatch(r"ip/([bwd])", word)
    if match:
        if len(tokens) != 2:
            raise CommandError("expected format: ip/<size> <address>")
        return PortCommand(_address(tokens[1]), {"b": 1, "w": 2, "d": 4}[match[1]])
    raise CommandError(f"invalid command `{line}`")


def _address(value: str) -> int:
    try:
        address = int(value, 16)
    except ValueError as error:
        raise CommandError(f"invalid hexadecimal address `{value}`") from error
    if address not in range(1 << 64):
        raise CommandError("address is outside the 64-bit range")
    return address
