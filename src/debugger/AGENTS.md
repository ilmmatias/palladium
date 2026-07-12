# Debugger client guidance

This Python 3 package is the host TUI/client for the kernel's custom UDP debugger protocol. Run it
from the repository root as:

```sh
python3 -m src.debugger <ipv4-or-localhost> <port>
```

- `codec.py` constants and little-endian `struct` formats mirror
  `src/kernel/include/private/kernel/detail/kdpdefs.h`, `kdptypes.h`, and `kernel/kd/protocol.c`.
  Update both sides atomically for a wire change; do not add client-only packet types that imply
  target support.
- Validate datagram length and field relationships before unpacking or indexing. Keep the current
  outstanding-request state coherent on timeout, malformed reply, unexpected ACK, and user exit.
- `transport.py` owns peer-validated UDP I/O, `session.py` owns handshake and request state,
  `commands.py` parses typed commands, and `events.py` is the frontend boundary. `interface.py` owns
  curses while `headless.py` renders text/JSONL. Do not fold wire behavior into presentation.
- Always restore the curses terminal and close the socket on error/interrupt paths. Export-log paths
  are user-selected host writes; do not hard-code machine paths or write during passive inspection.
- The TUI imports `curses`, and disassembly lazily imports `capstone`; headless paths that do not
  disassemble must remain usable without either module. Disassembly currently maps only `amd64`.

Host tests under `tests` use a synchronized localhost fake target. They cover the Python client but
not the kernel's raw Ethernet/KD extension path. Also connect to a debugger-enabled QEMU boot when
kernel wire behavior changes, and never commit generated `__pycache__` data.
