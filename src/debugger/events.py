# SPDX-FileCopyrightText: (C) 2025-2026 ilmmatias
# SPDX-License-Identifier: GPL-3.0-or-later

"""Frontend-neutral events produced by the debugger client."""

from dataclasses import dataclass
from typing import Any


@dataclass(frozen=True)
class Event:
    kind: str
    data: dict[str, Any]


def event(kind: str, **data: Any) -> Event:
    return Event(kind, data)
