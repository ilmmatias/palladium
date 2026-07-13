# ARM64 architecture plan

Evidence labels are defined in [the roadmap index](README.md). The intended first target in this
plan is QEMU `virt` under AArch64 UEFI with ACPI. That is a recommendation pending the platform
blocking question; it is not a claim that device-tree support exists or is needed.

The governing primary references are [UEFI 2.11](https://uefi.org/specs/UEFI/2.11/) for AArch64
firmware entry and boot services, the [Arm Architecture Reference Manual for A-profile](https://developer.arm.com/documentation/ddi0602/latest/),
the [Arm Generic Interrupt Controller architecture](https://developer.arm.com/documentation/ihi0069/latest/),
and [PSCI](https://developer.arm.com/documentation/den0022/latest/). ARM64 PE images and unwind
records must also follow the [Microsoft PE/COFF format](https://learn.microsoft.com/en-us/windows/win32/debug/pe-format)
and [ARM64 exception-handling ABI](https://learn.microsoft.com/en-us/cpp/build/arm64-exception-handling).
[SPEC]

## 8. amd64 assumption map

| Category | Current repository evidence | Smallest two-architecture response |
|---|---|---|
| Build-system only | `src/CMakeLists.txt` accepts only `ARCH=amd64`; `src/sdk/cmake/project.cmake` selects x86_64 compiler-rt, `-mcx16`, no red zone, amd64 include paths and PE linker settings; CI and `run.sh` name `BOOTX64.EFI` and `qemu-system-x86_64` | Add per-architecture target descriptors for compiler target, compiler-rt name, EFI filename, QEMU profile, sources and flags. Keep common target-kind rules common. Do not claim ARM64 until its complete target graph links. |
| Bootloader | `src/boot/osloader/amd64/{page,support,transfer}.c`; PE validation expects amd64 machine; transfer manipulates CR0/CR3/CR4/EFER/PAT and Microsoft x64 RCX handoff; architecture boot data carries cycle counter facts | Add `arm64` mechanisms for PE machine, AArch64 UEFI entry ABI, translation tables, EL state, cache/TLB and handoff. Version common/arch loader-block pieces; validate magic/version in kernel. |
| Kernel entry | PE entry is `KiSystemStartup`; amd64 startup switches to a kernel stack in `HalpInitializeBootStack`; AP assembly re-enters it | Define a tiny arch entry that normalizes firmware/AP register state into `KiSystemStartup(common_loader, cpu_local)`. Keep generic startup order in `ke/entry.c`. |
| Page tables/address space | Four-level 4-KiB x86 page tables, recursive PML4, x86 PTE flags and canonical-address rules in loader/kernel map code; `docs/amd64/AddressSpace.txt` | Preserve the logical regions where practical but define an ARM64 layout document from chosen VA size/granule. Use architecture map ops and common protection intent, not common raw PTE bits or a forced recursive map. |
| Context switching | amd64 `context.S`, interrupt-frame/C offsets and `RtSaveContext`/`RtRestoreContext`; `KeProcessor` embeds x86 stacks/GDT/IDT | Split common scheduler fields from arch processor state. Define only create/switch/restore/context-inspect ops required by both ports; synchronize assembly offsets with static assertions/generated include. |
| Exceptions/interrupts | `idt.S/c`, GDT/TSS/IST, x86 exception codes, CR8 IRQL, APIC vectors; `HalInterruptFrame` is amd64 public detail | Add ARM64 vector table and exception syndrome/fault decoding; make protocol context independent. Represent executive IRQL semantically and implement via GIC priority/DAIF policy, not x86 vector imitation. |
| Atomics/memory ordering | Clang atomics, x86 spin implementation and 16-byte `cmpxchg16b` atomic SList; several algorithms benefit from x86’s strong ordering | Inventory every atomic and barrier. Use C atomics where lock-free guarantees are established; provide arch primitives for CPU-local, barriers and SList. Specify acquire/release/full ordering at algorithm boundaries. |
| Timer | TSC/HPET selection and local APIC timer; loader seeds TSC frequency | Implement architectural Generic Timer counter/frequency and per-CPU timer interrupt. Keep common duration conversion and scheduler tick policy. ACPI GTDT may describe platform timers; do not require HPET. |
| Interrupt controller | LAPIC/IOAPIC, vectors, EOI, IPI and PCI interrupt routing | Add a narrow controller backend covering priority/IRQL, vector allocation, mask/unmask/EOI, SGI and GSI/SPI routing. Initial QEMU target chooses a concrete GIC version rather than an abstract “any controller.” |
| SMP | MADT local APIC records, low-memory `0x8000` trampoline and INIT/SIPI; `ApicId` stored in `KeProcessor` | Use ACPI GICC/MPIDR discovery and PSCI `CPU_ON`; make processor hardware ID architecture-owned. Preserve common online/runnable initialization after arch entry. |
| PCI/firmware | x86 legacy config mechanism is exposed through HAL; ACPI early parser understands amd64-relevant MADT/HPET; KD loader scans PCI handles via UEFI | Prefer ECAM described by MCFG for ARM64. Extend early ACPI parsing for GICC/GICD/GICR and GTDT. UEFI PCI I/O remains useful in loader. Avoid adding x86 port-I/O to common interfaces. |
| ACPI | Custom driver’s generic AML is mostly architecture-neutral; operation regions use SystemIO and PCI assumptions; HAL early ACPI parses x86 interrupt/timer structures | Keep interpreter common. Gate SystemIO on platform capability; make PCI config backend ECAM-capable; add ARM64 ACPI table records as concrete demand appears. |
| Debugger | architecture handshake knows amd64; client Capstone mapping is amd64; port-read request is x86; stop contexts absent; KD device is PCI network | Version architecture contexts; capability-gate port I/O; add AArch64 register/ESR/FAR context and BRK/single-step only after generic stop controller. Transport remains architecture-neutral. |
| CRT/RT/ABI | common CRT is portable C; kernel RT context/SEH/unwind is amd64; atomic SList requires 16-byte x86 primitive; loader/kernel use Microsoft x64 conventions | Compile/test common CRT first. Add ARM64 context and PE unwind per ABI as a separate phase. Do not map amd64 `CONTEXT`/unwind structs onto ARM64. Replace or specialize atomic SList with explicit layout/alignment guarantees. |

Additional embedded assumptions include `KeIrql`/`KeSpinLock` widths, PIO helpers, CPUID feature
gates, SSE use in optimized memory/string paths, PAT cache modes, MSRs, non-maskable freeze, and
APIC ID fields in structures visible outside `hal/amd64`. [CODE] The inventory should be made a
checked architecture-port worksheet before moving files; a directory name alone does not prove the
generic layer is neutral.

## Minimal architecture-neutral contracts

**[REC]** Add only seams justified by both concrete ports:

1. **Build target descriptor:** target triple/PE machine, EFI fallback filename, compiler-rt name,
   architecture sources, required features and emulator profile.
2. **Loader handoff:** a common fixed header (`magic`, version, total size, common size, arch ID,
   arch offset/size), common pointer/list/framebuffer/ACPI/debug records, and an architecture payload.
   Both sides assert packing, alignment, offsets and pointer width. Unknown versions are rejected
   before dereference.
3. **CPU-local split:** common `KeProcessor` scheduler/MM cache/accounting fields plus opaque or
   architecture-included `KeArchProcessor` containing hardware ID, exception tables/stacks and
   controller state. Avoid heap indirection on the hot path unless measurement demands it.
4. **Execution context:** create initial thread context, switch/restore context, capture debugger
   context. The private scheduler frame and public debugger wire context remain different types.
5. **MMU operations:** establish early map, map/unmap/protect/translate with common semantic flags,
   local/global TLB synchronization and executable-code synchronization. Page-table allocation
   stays MM-owned; descriptor encoding stays architecture-owned.
6. **Interrupt/IRQL operations:** current level, raise/lower, interrupt enable state, controller
   mask/unmask/EOI, allocate route/vector and send IPI. Preserve Palladium’s IRQL semantics; do not
   import Linux IRQ domains.
7. **Clock/timer:** monotonic counter/frequency and per-CPU deadline/periodic tick programming.
8. **Processor discovery/start:** enumerate firmware hardware IDs, start one processor with entry/
   context, notify another processor, reversible debugger rendezvous.
9. **Firmware PCI:** common segment/bus/device/function configuration API backed by current amd64
   path or ARM64 ECAM. Port I/O remains architecture capability, not generic PCI policy.
10. **Low-level ordering/atomics:** named barriers and CPU relax/wait primitives only where C atomics
    cannot express a required architectural operation.

Do not introduce generic cache, NUMA, IOMMU, device-tree, power-management or interrupt-remapping
frameworks before a second implementation requires them. Keep current public HAL interrupt calls
stable for the ACPI boot driver where semantics remain valid; change packed/public structures only
through reviewed ABI commits.

## Staged ARM64 milestones

Every stage keeps amd64 configured and built in the same change series.

1. **R0 — Compile graph:** `ARCH=arm64` selects a target triple, compiler-rt builtins and empty
   architecture source sets; common CRT/RT libraries compile. No “supported” claim.
2. **R1 — UEFI diagnostic image:** `BOOTAA64.EFI` enters under QEMU/EDK2, uses UEFI console or GOP
   for a loader marker, validates PE machine and halts cleanly. It need not load the kernel yet.
3. **R2 — Kernel PE handoff:** loader reads config, loads/relocates an ARM64 kernel, constructs and
   validates the new loader block, exits boot services and reaches an ARM64-owned early entry.
4. **R3 — EL1/high-half banner:** establish chosen translation regime, stack, CPU-local pointer,
   exception vectors and framebuffer/KD-independent diagnostic sink; enter common
   `KiSystemStartup` far enough to print the banner.
5. **R4 — Single-core executive:** Generic Timer and GIC interrupts work; page/pool allocators and
   scheduler run idle/system threads on one CPU; ACPI driver loads to the supported subset.
6. **R5 — PSCI SMP:** MADT GICC discovery and PSCI bring APs online; scheduler/event/MM SMP stress
   matches amd64 gates.
7. **R6 — Platform parity:** PCI ECAM/routing, operation-region needs, boot reliability and fatal/
   unwind diagnostics meet the amd64 evidence contract.
8. **R7 — Debug parity:** v1 AArch64 context, stop/continue/single-step and BRK software breakpoints;
   KD transport proven on a concrete ARM64 virtual/physical NIC.

“ARM64 platform parity” means parity with Palladium’s present amd64 boot-driver/runtime scope, not
with a general-purpose OS hardware-support matrix.

## Detailed epic plans

### Epic R1: HAL and loader architecture contract

**Objective / done.** The contracts above are documented and implemented with no amd64 behavior
change. Architecture-specific state no longer leaks into common code except through deliberate
detail includes. amd64 clean build, smoke boot and SMP stress remain green; ABI layout assertions
cover loader/context boundaries.

**Source.** root and `src/sdk/cmake` CMake; loader `include/platform.h`, `amd64/*`, PE code; kernel
`include/{public,private}/kernel/detail`, `ke/entry.c`, `hal/*`, `mm/map.c`, `ps/{thread,scheduler}.c`
and `hal/amd64/context.*`; SDK RT context/atomic headers. Update `docs/amd64/AddressSpace.txt` only if
its actual contract changes.

**Decisions.** loader-block v7 compatibility and rejection, common/arch `KeProcessor` layout,
semantic map flags, IRQL/controller boundary, context-switch ABI and compile-time offset generation.

**Phases / commits.** (1) architecture inventory and RFC. (2) loader-block header/version validation
without layout move. (3) split CPU common/arch fields. (4) map/TLB/code-sync ops. (5) interrupt,
timer and processor-start ops. (6) build target descriptor. Each phase must be behavior-preserving
on amd64 and independently reviewable.

**Tests first.** ABI `sizeof/offsetof` static assertions; malformed loader version/size test; amd64
1/4-CPU boot; scheduler/MM stress; compile both common target graphs. **Risks:** circular headers,
hot-path regressions, silently changed packed structures/assembly offsets, abstracting x86 names
instead of semantics. Depends on M0/M1. Do not combine with ARM64 mechanisms or breakpoint exception
work. **Model:** Sol/xhigh design, Sol/high mechanical split. **Subagents:** useful for separate
inventory/review of loader, HAL and RT after one RFC owner; file ownership must be disjoint.

### Epic R2: ARM64 UEFI loader

**Objective / done.** QEMU `virt` firmware starts `BOOTAA64.EFI`; the loader obtains configuration,
ACPI/GOP/memory map, validates and relocates an ARM64 PE kernel, exits boot services and transfers a
validated loader block to the kernel entry. All error paths remain in firmware-safe diagnostics
before exit.

**Source.** new `src/boot/osloader/arm64`; common loader CMake, PE, allocation, file/config/graphics,
EFI headers and `include/platform.h`; image builder and CI QEMU profile.

**Decisions.** AArch64 UEFI calling convention and entry thunk, PE relocation types actually
emitted by Clang/LLD, translation granule/VA size, initial identity/high mapping, cache/TLB sequence,
cycle-counter handoff, `BOOTAA64.EFI` image layout. Confirm every choice against UEFI/Arm/PE specs.

**Phases.** (1) UEFI hello/error image. (2) ARM64 PE validation and synthetic relocation tests.
(3) config/GOP/ACPI parity. (4) loader-block creation and kernel load. (5) page tables/ExitBootServices
handoff. (6) deterministic QEMU marker gate.

**Tests first.** Host PE parser fixtures for wrong machine/relocations/overflow; mockable memory-map
growth and `ExitBootServices` retry; QEMU fresh-vars boot; forced missing GOP/ACPI/config failures.
**Risks:** firmware memory attributes, cache maintenance, relocation misunderstandings, page-table
alignment and ABI register mismatch. Depends on R1 and V1. Commit entry, PE, services and handoff
separately. Do not add GIC/timer/SMP here. **Model:** Sol/xhigh for handoff/MMU, Sol/high for common
service parity. **Subagents:** helpful for PE fixture/test work versus UEFI entry with strict file
ownership; final handoff reviewed centrally.

### Epic R3: ARM64 early kernel and single-core HAL

**Objective / done.** ARM64 reaches common startup, initializes early/MM pool state, installs
exceptions, handles timer interrupts and schedules idle/system threads on one CPU. Expected fatal
faults produce architecture context rather than a silent hang.

**Source.** new `src/kernel/hal/arm64` and architecture detail headers; `ke/entry.c`, MM map flags,
PS context; new SDK RT ARM64 context/atomic implementation; ACPI early table extensions; video or
structured diagnostic sink.

**Decisions.** EL1 register policy, VA/PA sizes and 4-KiB granule, exception stack layout, CPU-local
register, GIC version for QEMU, priority-to-IRQL mapping, Generic Timer mode, initial unwind scope.

**Phases.** (1) stack/EL1/vector fatal trap. (2) high-half mapper and TLB/code sync. (3) common MM/
pool initialization. (4) GIC distributor/redistributor and interrupt dispatch. (5) Generic Timer.
(6) thread context create/switch and single-core scheduler. (7) ACPI boot driver subset.

**Tests first.** page-map golden address/flag checks, intentional synchronous exceptions, timer
monotonicity and tick count, allocator stress, context-switch register canaries, one-CPU QEMU boot
markers. **Risks:** exception recursion, priority masking errors, lost EOI, weak-ordering bugs,
unwind unavailable during early faults. Depends on R1/R2 and C1–C3 gates. Keep MMU, exceptions,
timer and scheduler in separate commits. Do not add SMP concurrently. **Model:** Sol/xhigh; Luna/
xhigh independent register/bitfield tests. **Subagents:** useful across MMU and timer/GIC only after
the shared context/IRQL contract is frozen.

### Epic R4: ARM64 interrupts, timer, SMP and debugger

**Objective / done.** This epic is a program with separate deliverables: production-quality GIC/
timer routing; PSCI AP bring-up; 1/2/4-CPU executive stress; ACPI/PCI platform parity; then debugger
context, stop/resume, single-step and BRK breakpoints. Completion requires QEMU plus stated hardware
gaps, not a blanket hardware-support claim.

**Source.** ARM64 HAL exception/GIC/timer/SMP/ACPI/PCI modules, generic KE IPI/synchronization, PS/
EV/MM SMP paths, SDK RT ARM64 unwind/context, KD context/breakpoint architecture backend, debugger
Capstone/context rendering.

**Decisions.** GIC version and redistributor discovery, PSCI conduit/return handling, MPIDR mapping,
SGI allocation, secondary entry cache coherency, ECAM/routing, BRK immediate ownership, architectural
single-step/debug registers, I-cache maintenance on code patch.

**Phases / merge boundaries.** (1) robust controller/timer and interrupt storm tests. (2) firmware
CPU enumeration. (3) PSCI one AP then N APs. (4) scheduler/MM/event stress. (5) PCI/ACPI parity.
(6) context and stop. (7) BRK patch/step. (8) RT unwind/fatal diagnostics. Never make this one PR.

**Tests first.** PSCI failure/timeout, duplicate MPIDR, missing CPU, SGI ping, per-CPU timer,
allocation/wait/termination stress, simultaneous breakpoint hits, instruction-cache maintenance,
disconnect mid-step. **Risks:** memory ordering visible only under SMP, GIC priority/IRQL inversion,
firmware table variance, debugger stop deadlocks. Depends on R3, D2/D3 architecture-independent
stop controller, and M1. **Model:** Sol/xhigh throughout; max only for a real Arm/PE unwind
ambiguity. **Subagents:** strongly useful with explicit ownership (GIC/timer, PSCI/SMP, KD/RT tests)
after shared ABI decisions; do not allow concurrent edits to common scheduler or protocol ABI.
