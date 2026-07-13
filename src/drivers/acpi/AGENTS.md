# ACPI/AML driver guidance

This subtree is Palladium's custom ACPI boot driver and AML interpreter. Preserve that direction;
do not replace it with ACPICA, uACPI, or a host firmware abstraction. Build the `acpi` target and
boot with `BOOT_DRIVERS="acpi"` for runtime verification.

- Public driver ABI is `include/public` plus `acpi.def`; interpreter/OS details stay under
  `include/private`. Review kernel and future-driver consumers before changing `AcpiValue`,
  `AcpiObject`, evaluation/casting semantics, or exports.
- AML bytes and ACPI tables are untrusted firmware input. All opcode, package-length, string,
  field, name, table, region, offset, and width reads must remain bounded; use `memcpy` for
  potentially unaligned multi-byte AML/buffer access. Check arithmetic before advancing the stream
  or allocating. Unsupported opcodes must remain explicit, not be treated as successful no-ops.
- `AcpipState`, scopes, opcodes, locals, arguments, namespace objects, and returned `AcpiValue`s
  have distinct lifetimes. Preserve `AcpiCreateReference`/`AcpiCopyValue`/
  `AcpiRemoveReference` semantics for every value kind, including nested packages, fields, aliases,
  mutexes, and events. Clean temporary method scopes and partial allocations on every early return.
- Allocate/free interpreter memory only through `AcpipAllocateBlock`/`AcpipAllocateZeroBlock` and
  `AcpipFreeBlock`, which pair with the ACPI pool tag. Kernel event/mutex handles created by the OS
  layer also carry kernel object lifetimes; verify their ownership before adding cleanup.
- Keep AML parsing/execution in `interp`/`opcode`, operation-region behavior in `region`, and kernel
  adaptation in `os`. Do not embed amd64 port/PCI assumptions into generic interpreter logic;
  current PCI segment limitations and `_PIC` placement are explicit incomplete paths, not support
  guarantees.
- Expected evaluation/type/allocation failures use boolean cleanup paths. Corrupt mandatory firmware
  during boot uses `AcpipShowErrorMessage`, which is fatal; do not make optional or attacker-controlled
  cases fatal without reviewing the initialization contract.
- Preserve ACPI ordering and units: namespace population before device initialization, global-lock
  setup, `_STA`/`_INI`/`_REG`, timeout conversions, access widths, and field update rules.

Verification: build `acpi.sys`, boot through QEMU, and confirm `enabled ACPI` with no driver fatal
error. Exercise the changed AML/table path using relevant firmware when possible, including a
malformed/truncated case. QEMU does not establish real-hardware EC/CMOS/PCI or broad AML coverage;
report those gaps.
