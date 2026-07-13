# Kernel debugger guidance

This is the kernel side of KD: external KDNET module integration, polling transport, Ethernet/ARP/
IPv4/UDP framing, the custom debug protocol, and kernel print/break handling.

- WDK-compatible structures, import/export counts, flags, callbacks, and status semantics in
  `include/private/kernel/detail/kdp*.h` must remain synchronized with the legally obtained Windows
  KDNET module generation supported by the project. KDNET callbacks return `STATUS_SUCCESS` as zero;
  do not reinterpret them as kernel booleans. Never add the proprietary modules to the repository.
- The loader discovers and maps the external module; `initialize.c` initializes it; `device.c`,
  `import.c`, and `export.c` adapt it; Ethernet/IP/UDP files frame transport; `protocol.c` implements
  requests. Preserve these boundaries when changing behavior.
- Custom packet types and packed layouts in `kdpdefs.h`/`kdptypes.h` must match
  `src/debugger/protocol.py` and its `struct` formats exactly. Packet-type, field, endian, width,
  header-size, response-size, and failure-signaling changes are protocol changes and require both
  sides to change together.
- Treat every received frame and packet as hostile: validate minimum and declared lengths,
  multiplication/addition overflow, allowed item sizes, address wrap, source peer/state, and the
  fixed 1024-byte response buffer before access. Invalid requests are ignored or receive the
  established failure form; do not fatal-error the kernel for malformed network input.
- Preserve every RX handle release and TX/map/unmap cleanup path, including exceptions and failed
  sends. Debug memory/port reads run in a sensitive break/panic environment; keep bounded SEH access
  and avoid dependencies on normal scheduler or allocator progress.
- The controller is polling with interrupts masked, and early debugger initialization intentionally
  blocks until handshake. Keep protocol changes separate from print colors/TUI presentation and
  from QEMU host-forwarding configuration.

Verification: build the kernel, boot with the documented `BOOT_DEBUG_*` settings and an appropriate
QEMU NIC/KDNET module, connect with `python3 -m src.debugger localhost <port>`, and exercise handshake,
print, break, and each changed request plus a malformed/oversized case. Check `net0.pcap` only as a
local diagnostic; never commit it.
