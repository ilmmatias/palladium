# OS loader guidance

This scope is the amd64 UEFI application and PE loader. Build it through the repository build; boot
changes require rebuilding the image and, when practical, running `../run.sh` from the build
directory with `BOOT_DRIVERS="acpi"`.

- UEFI is the supported generic amd64 boot contract; do not add BIOS or Unix boot assumptions.
- `include/platform.h` and `include/amd64/platform.h` define the packed `OslpBootBlock` passed to
  the kernel. Their kernel mirrors are
  `src/kernel/include/private/kernel/detail/kitypes.h` and its amd64 counterpart. Change both sides,
  and preserve field widths/order/packing. `OSLP_BOOT_VERSION` is written into the block, but no
  kernel-side version validation is currently present; do not assume changing that constant alone
  makes an incompatible contract safe.
- PE headers come from `src/sdk/include/os/pe.h`. Preserve validation of DOS/PE offsets, sections,
  data-directory ranges, imports, exports, relocations, load-config cookies, sizes, alignment, and
  integer overflow before dereferencing image data. Unsupported imports/relocations must not be
  accepted silently.
- Pair firmware `AllocatePool`/`AllocatePages` with the matching boot-services free operation on
  failure paths. After the descriptor list is built, every important allocation must be reflected
  through `OslpUpdateMemoryDescriptors` as `main.c` requires.
- Preserve the final memory-map refresh/retry and `ExitBootServices` ordering. Do not call boot
  services, print through firmware, or allocate through firmware after a successful exit; keep
  interrupts disabled while installing the new amd64 execution environment.
- Keep architecture-specific page-table construction, feature checks, and transfer assembly under
  `amd64`. Reconcile address changes with `docs/amd64/AddressSpace.txt` and the kernel HAL/MM users.
- Boot configuration, PE files, ACPI tables, PCI data, and externally supplied KDNET modules are
  untrusted. Reject malformed or missing data with existing loader diagnostics rather than relying
  on assertions or partial initialization.
- KDNET discovery/loading is distinct from the kernel transport and protocol. The loader accepts
  only the expected no-import module with `KdInitializeLibrary`; never vendor that proprietary DLL.
- Treat `include/efi` as EDK2-derived vendored headers: retain licensing, packing, and local
  `clang-format off`; avoid project-wide style rewrites there.

Verification: build `osloader`, create the EFI/ISO image, and confirm the loader reaches the kernel
without its `Failed to ...`/`cannot continue` diagnostics. Exercise missing/corrupt input paths for
parser or PE-validation changes; QEMU success alone does not cover real-firmware memory-map races.
