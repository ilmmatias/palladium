# Debugger client guidance

This Python 3 package is the host TUI/client for the kernel's custom UDP debugger protocol. Run it
from the repository root as:

```sh
python3 -m src.debugger <ipv4-or-localhost> <port>
```

- `protocol.py` constants and little-endian `struct` formats mirror
  `src/kernel/include/private/kernel/detail/kdpdefs.h`, `kdptypes.h`, and `kernel/kd/protocol.c`.
  Update both sides atomically for a wire change; do not add client-only packet types that imply
  target support.
- Validate datagram length and field relationships before unpacking or indexing. Keep the current
  outstanding-request state coherent on timeout, malformed reply, unexpected ACK, and user exit.
- `connection.py` owns handshake, `receiver.py` dispatches packets, `command.py` parses/sends
  commands, `utils.py` renders memory/disassembly, and `interface.py` owns curses. Do not fold wire
  behavior into TUI presentation.
- Always restore the curses terminal and close the socket on error/interrupt paths. Export-log paths
  are user-selected host writes; do not hard-code machine paths or write during passive inspection.
- The source imports `curses` and `capstone`; the README currently mentions only curses, so do not
  claim dependency installation is fully documented. Disassembly currently maps only `amd64`.

No automated client tests are present. At minimum check changed modules for Python syntax without
creating committed `__pycache__`; for behavior changes, connect to a debugger-enabled QEMU boot and
exercise the changed command, timeout/error handling, resize/exit behavior, and malformed datagrams.
