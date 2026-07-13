# SPDX-FileCopyrightText: (C) 2025-2026 ilmmatias
# SPDX-License-Identifier: GPL-3.0-or-later

"""Typed command service shared by headless and curses frontends."""

from __future__ import annotations

from .commands import (BreakpointCommand, ContextCommand, CpusCommand, ExecutionCommand,
                       MemoryCommand, PortCommand, StatusCommand)


class CommandService:
    def __init__(self, session):
        self.session = session

    def execute(self, command, timeout: float):
        return self.session.execute(command, timeout)

    def read_memory(self, physical: bool, address: int, item_size: int, count: int,
                    timeout: float):
        return self.execute(MemoryCommand(physical, False, address, item_size, count), timeout)

    def read_port(self, address: int, item_size: int, timeout: float):
        return self.execute(PortCommand(address, item_size), timeout)

    def status(self, timeout: float):
        return self.execute(StatusCommand(), timeout)

    def cpus(self, timeout: float):
        return self.execute(CpusCommand(), timeout)

    def context(self, processor: int = 0, extended: bool = False, timeout: float = 5.0):
        return self.execute(ContextCommand(processor, extended), timeout)

    def stop(self, timeout: float):
        return self.execute(ExecutionCommand("stop"), timeout)

    def continue_(self, timeout: float):
        return self.execute(ExecutionCommand("continue"), timeout)

    continue_execution = continue_

    def step(self, timeout: float):
        return self.execute(ExecutionCommand("step"), timeout)

    def breakpoint_add(self, address: int, timeout: float):
        return self.execute(BreakpointCommand("add", address), timeout)

    def breakpoint_list(self, timeout: float):
        return self.execute(BreakpointCommand("list"), timeout)

    def breakpoint_enable(self, identifier: int, timeout: float):
        return self.execute(BreakpointCommand("enable", identifier), timeout)

    def breakpoint_disable(self, identifier: int, timeout: float):
        return self.execute(BreakpointCommand("disable", identifier), timeout)

    def breakpoint_remove(self, identifier: int, timeout: float):
        return self.execute(BreakpointCommand("remove", identifier), timeout)
