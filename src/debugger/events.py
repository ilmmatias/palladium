# SPDX-FileCopyrightText: (C) 2025-2026 ilmmatias
# SPDX-License-Identifier: GPL-3.0-or-later

"""Frontend-neutral events emitted by :class:`DebugSession`."""

from dataclasses import dataclass
from typing import Any


@dataclass(frozen=True)
class Event:
    kind: str
    data: dict[str, Any]


def event(kind: str, **data: Any) -> Event:
    return Event(kind, data)


def connection(*, session_id: int, architecture: int, context_version: int,
               capabilities: int, processor_count: int) -> Event:
    return event("connection", state="connected", protocol_version=1, session_id=session_id,
                 architecture=architecture, context_version=context_version,
                 capabilities=capabilities, processor_count=processor_count)


def diagnostic(level: str, code: str, message: str, **extra: Any) -> Event:
    return event("diagnostic", level=level, code=code, message=message, **extra)


def target_output(text: str, severity: int = 0) -> Event:
    return event("target_output", text=text, severity=severity)


def stopped(reason: int, *, processor: int, address: int, resumable: bool,
            context: dict[str, Any] | None = None) -> Event:
    return event("stop", reason=reason, processor=processor, address=address,
                 resumable=resumable, context=context)
