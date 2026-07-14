# Palladium repository instructions

Palladium is a custom, modular/hybrid, NT-inspired operating system. It is neither
source- nor binary-compatible with Windows. Preserve its existing custom kernel,
HAL, ACPI/AML interpreter, CRT/RT, loader, and debugger direction.

Currently the only supported target is amd64.

## Repository map

- `src/boot/osloader`: UEFI loader, PE loading, boot configuration, page tables,
  loader-to-kernel handoff, and optional KDNET module discovery.
- `src/kernel/hal`: architecture HAL, MMU, interrupt, timer, PCI, SMP, and context
  machinery.
- `src/kernel/{ke,ps,ev,ob,mm}`: executive, scheduler, synchronization, objects,
  and memory management.
- `src/kernel/kd`: target-side debugger transport/protocol code.
- `src/debugger`: Python host debugger.
- `src/drivers/acpi`: custom ACPI table, namespace, AML, operation-region driver.
- `src/sdk/crt`: freestanding C runtime.
- `src/sdk/rt`: intrusive containers, atomics, contexts, exceptions, and unwind.
- `src/sdk/include` and subsystem `include/public`: public ABI.
- `include/private` and names ending in `p`: internal implementation contracts.

## Canonical commands

Bootstrap compiler-rt exactly as documented:

`PALLADIUM_PATH="$PWD" COMPILER_RT_PATH=llvm-project/compiler-rt COMPILER_RT_TARGET=x86_64 tools/build-compiler-rt.sh`

Configure and build exactly as documented:

`cmake -Ssrc -Bbuild.amd64.rel -GNinja -DCMAKE_BUILD_TYPE=Release -DARCH=amd64 && cmake --build build.amd64.rel`
or
`cmake -Ssrc -Bbuild.amd64.reldbg -GNinja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DARCH=amd64 && cmake --build build.amd64.reldbg`
or
`cmake -Ssrc -Bbuild.amd64.dbg -GNinja -DCMAKE_BUILD_TYPE=Debug -DARCH=amd64 && cmake --build build.amd64.dbg`

For clang-tidy, configure with `CMAKE_EXPORT_COMPILE_COMMANDS=ON`, then run:

`run-clang-tidy -p <build-directory> -source-filter='.*\.(c)$'`

Do not run repository-wide `run-clang-tidy -fix`. Classify findings and make
reviewable, subsystem-scoped fixes.

The documented manual QEMU invocation is:

`tools/build-image.sh --build-dir <build-directory> --output-dir <build-directory>/image`
followed by
`tools/run-qemu.sh --iso <build-directory>/image/iso9660.iso --ovmf-code <path> --ovmf-vars <path>`

The current host debugger command is:

`python3 -m src.debugger <ip> <port>`

There is no canonical automated test or CTest command as of yet.

## ABI and architecture discipline

- Loader blocks, PE structures, exported `.def` files, public structures, wire
  packets, C/assembly context layouts, interrupt frames, and unwind records are ABI.
- Update every mirrored definition together.
- Never serialize a private in-memory interrupt or context structure directly.
- Keep architecture representation below a versioned semantic HAL contract.
- Do not scatter new architecture `#ifdef`s through generic executive code.
- Treat firmware tables, PE images, config files, AML, device data, and debugger
  packets as hostile input.
- All size additions, multiplications, alignments, index calculations, shifts,
  address-range calculations, and packet lengths require checked arithmetic.
- Every allocation/mapping sequence must have complete reverse-order rollback.
- Preserve W^X and explicit TLB/instruction-cache synchronization.

## Concurrency and privilege rules

- Document the required IRQL, locks held, lock order, ownership, and blocking
  behavior for every new or changed low-level routine.
- `*AtCurrentIrql` helpers do not raise or validate IRQL.
- Always restore the exact prior IRQL.
- Do not block, allocate through a blocking path, or invoke untrusted callbacks
  while holding a spin lock.
- Preserve object references across unlocked use.
- Treat queue membership, thread state, timeout membership, processor ownership,
  and reference counts as one transaction.
- Panic-only lock bypasses are valid only after other processors are known frozen.

## Style and naming

- Follow `.clang-format`: Google base, four spaces, no tabs, 100 columns, right
  pointer/reference alignment, and no short one-line control forms.
- Public subsystem entry points use prefixes such as `Ke`, `Mm`, `Ps`, `Ev`,
  `Ob`, `Hal`, `Kd`, `Acpi`, `Rt`, and `Osl`.
- Private helpers normally add the subsystem `p`: `Kep`, `Mip`, `Psp`, `Evp`,
  `Obp`, `Halp`, `Kdp`, `Acpip`, and `Oslp`.
- Types, fields, C functions, and local variables generally use PascalCase;
  constants/macros use uppercase snake case.
- Standard C runtime APIs retain their standard lowercase names.
- Lowercase filenames and existing header-routing conventions are intentional.
- C functions should be precedeed by a documentation header,
  in the same format as the ones already available in the repository.

## Generated and external material

Do not commit build directories, FAT/ISO images, OVMF vars/code files, boot configs,
packet captures, logs, proprietary KDNET modules, machine-specific paths, credentials,
or tokens.

Vendored EDK2 definitions and generated font data are not style-refactor targets
unless the task specifically concerns them.

## Definition of Done

- The change has an explicit subsystem and ABI/privilege impact assessment.
- Required IRQL, lock order, ownership, lifetime, and failure rollback are stated.
- Mirrored public/private and C/assembly layouts remain synchronized.
- The documented target build succeeds with the default `-Wall -Wextra -Werror`.
- Relevant focused tests pass, if such tests exist.
- New clang-tidy output is analyzed, and if deemed important, fixed.
- Malformed-input and allocation/mapping failure paths are exercised.
- SMP-sensitive work is checked on at least 1-CPU and 4-CPU configurations.
- Generated artifacts and unrelated user changes are absent from the diff.
- Documentation describes current behavior, not an unmerged branch or aspiration.
