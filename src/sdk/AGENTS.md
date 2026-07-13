# SDK, CRT, and RT guidance

This scope provides shared PE/intrinsic headers and the boot (`brt`), kernel (`kcrt`, `krt`), and
user (`ucrt`, `urt`, `ucrypt`, `osapi`) runtime variants. Build through the repository CMake graph;
there are no standalone SDK tests.

- C23 project headers and runtime implementations are intentional custom code. Preserve standard
  edge-case, width, overflow, aliasing, errno/fenv, bounded-format, and null-termination behavior;
  do not substitute host-libc behavior or link host libraries.
- `crt/CMakeLists.txt` shares core sources, then `kernel.cmake`/`user.cmake` add environment-specific
  startup and OS hooks. `rt` similarly shares data structures while adding kernel exception/image
  support. Keep boot/kernel/user dependencies and freestanding constraints separated.
- Headers under `crt/include`, `rt/include`, `crypt/include/public`, and `sdk/include` are consumer
  ABI. The `*.def` files are explicit DLL/kernel export surfaces. Update declarations,
  implementations, all relevant variants, and export lists together; review symbol spelling and
  duplicates instead of assuming the linker will diagnose an ABI mistake.
- `sdk/include/os/pe.h` is shared with the loader/runtime image parser. Preserve packed PE field
  widths and validate all consumers before changing it.
- amd64 `context.S`, exception dispatch, unwind parsing, context structures, and `.seh_*` metadata
  implement the PE/Windows-style calling and unwind contract. Preserve 16-byte alignment,
  nonvolatile-register rules, unwind opcode bounds, and paired C/assembly offsets. Keep amd64
  optimized `memcpy`/`memset` selected only by the existing architecture branch.
- `RtAtomicSList` depends on 16-byte alignment and amd64 `cmpxchg16b` (`-mcx16` is set globally for
  amd64). Do not weaken atomic ordering, ABA tagging, exception guards, or intrusive-list ownership
  without auditing every boot/kernel/user consumer.
- Data-structure functions are intrusive: callers own node storage and linkage state. Preserve AVL
  parent/height/subtree-size updates and list sentinel conventions through all rotations/removals.
- Startup/load-config/stack-check files and `.def` exports affect PE loading, security-cookie setup,
  and entry points. Treat changes as boot/ABI changes and verify the resulting image, not only
  compilation.

Verification: build every affected variant, not just the first library that compiles. For public or
export changes inspect the produced symbol/export surface and boot the image when loader, startup,
exception, unwind, atomic, or kernel runtime behavior is involved. CRT numeric/string/stdio changes
need focused boundary vectors, but no harness is currently supplied; report manual coverage and
missing conformance tests.
