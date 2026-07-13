# SPDX-FileCopyrightText: (C) 2025-2026 ilmmatias
# SPDX-License-Identifier: GPL-3.0-or-later

"""Pure debugger command grammar.

Parsing produces typed values only.  Transport, protocol constants, capability checks, and
rendering belong to the command service/session/frontends respectively.
"""

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
class StatusCommand:
    pass


@dataclass(frozen=True)
class CpusCommand:
    pass


@dataclass(frozen=True)
class ContextCommand:
    processor: int = 0
    extended: bool = False


@dataclass(frozen=True)
class ExecutionCommand:
    action: str  # stop, continue, step


@dataclass(frozen=True)
class BreakpointCommand:
    action: str  # add, list, enable, disable, remove
    value: int | None = None

    @property
    def identifier(self) -> int | None:
        return self.value


@dataclass(frozen=True)
class LocalCommand:
    name: str
    arguments: tuple[str, ...] = ()


Command = Union[MemoryCommand, PortCommand, StatusCommand, CpusCommand, ContextCommand,
                ExecutionCommand, BreakpointCommand, LocalCommand]
RegistersCommand = ContextCommand
TargetStatusCommand = StatusCommand
CpuListCommand = CpusCommand
ContinueCommand = ExecutionCommand
MAX_MEMORY_PAYLOAD = 952  # v1 max payload (976) minus the 24-byte read-memory response prefix.


def parse(line: str) -> Command | None:
    try:
        tokens = shlex.split(line)
    except ValueError as error:
        raise CommandError(str(error)) from error
    if not tokens:
        return None
    word = tokens[0].lower()
    if word in ("q", "quit"):
        if len(tokens) != 1:
            raise CommandError(f"{word} takes no arguments")
        return LocalCommand("quit")
    if word in ("h", "help"):
        if len(tokens) != 1:
            raise CommandError(f"{word} takes no arguments")
        return LocalCommand("help")
    if word == "detach":
        if len(tokens) != 1:
            raise CommandError("detach takes no arguments")
        return LocalCommand("detach")
    if word == "el" or word.startswith("el/"):
        if len(tokens) != 2 or (word != "el" and word not in ("el/k", "el/c")):
            raise CommandError("expected format: el[/target] <path>")
        return LocalCommand("export", (word[3:] if word != "el" else "", tokens[1]))

    if word == "status":
        _no_args(tokens, word)
        return StatusCommand()
    if word == "cpus":
        _no_args(tokens, word)
        return CpusCommand()
    register_match = re.fullmatch(r"(r|rx)(?:/([0-9a-fx]+))?", word)
    if register_match:
        if len(tokens) > 2:
            raise CommandError(f"{register_match[1]} expects an optional processor")
        processor_text = register_match[2] if register_match[2] is not None else (tokens[1] if len(tokens) == 2 else None)
        processor = _identifier(processor_text) if processor_text is not None else 0
        return ContextCommand(processor, register_match[1] == "rx")
    if word in ("stop", "continue", "step"):
        _no_args(tokens, word)
        return ExecutionCommand(word)

    if word == "bp" or word.startswith("bp/"):
        if word == "bp":
            if len(tokens) != 2:
                raise CommandError("expected format: bp <address>")
            address = tokens[1]
        else:
            if len(tokens) != 1:
                raise CommandError("expected format: bp/<address>")
            address = word[3:]
        return BreakpointCommand("add", None if address.lower() == "@rip" else _address(address))
    if word == "bl":
        _no_args(tokens, word)
        return BreakpointCommand("list")
    if word in ("be", "bd", "bc") or re.fullmatch(r"(?:be|bd|bc)/[0-9a-fx]+", word):
        if "/" in word:
            action_word, value = word.split("/", 1)
            if len(tokens) != 1:
                raise CommandError(f"expected format: {action_word}/<breakpoint-id>")
        else:
            action_word = word
            if len(tokens) != 2:
                raise CommandError(f"expected format: {action_word} <breakpoint-id>")
            value = tokens[1]
        return BreakpointCommand({"be": "enable", "bd": "disable", "bc": "remove"}[action_word],
                                 _identifier(value))

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
        address = _address(tokens[1])
        if address >= 1 << 16:
            raise CommandError("port address is outside the 16-bit range")
        return PortCommand(address, {"b": 1, "w": 2, "d": 4}[match[1]])
    raise CommandError(f"invalid command `{line}`")


def _no_args(tokens: list[str], word: str) -> None:
    if len(tokens) != 1:
        raise CommandError(f"{word} takes no arguments")


def _identifier(value: str) -> int:
    try:
        identifier = int(value, 0)
    except ValueError:
        try:
            identifier = int(value, 16)
        except ValueError as error:
            raise CommandError(f"invalid identifier `{value}`") from error
    if identifier not in range(1 << 32):
        raise CommandError("identifier is outside the 32-bit range")
    return identifier


def _address(value: str) -> int:
    try:
        address = int(value, 16)
    except ValueError as error:
        raise CommandError(f"invalid hexadecimal address `{value}`") from error
    if address not in range(1 << 64):
        raise CommandError("address is outside the 64-bit range")
    return address


def __getattr__(name: str):
    if name == "CommandService":
        from .service import CommandService
        return CommandService
    raise AttributeError(name)
