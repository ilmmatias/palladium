# Core correctness, CRT/RT and verification plan

Evidence labels are defined in [the roadmap index](README.md). This is not a vague “audit the
kernel” project. Each pass has a bounded ownership model, executable invariants, stress/fault
scenarios and a stop condition.

## Verification baseline observed for this roadmap

On 2026-07-12, the existing `build.amd64.reldbg` tree built all configured targets successfully:
osloader, kernel, ACPI and SDK target variants. A repository-wide LLVM 22 `clang-tidy` run using the
available compile database reported 685 diagnostics. Most were warning-family volume rather than
685 demonstrated defects: 476 `bugprone-macro-parentheses`, 84 narrowing conversions, 21 missing
switch defaults and 19 signed-character warnings. [CODE/OBSERVED TOOL RESULT]

High-signal findings to reproduce and classify include:

- possible null `FileInfo` use in `src/boot/osloader/file.c`;
- graphics size multiplication widened only after 32-bit arithmetic;
- possible BAR array overrun in `src/kernel/kd/device.c` for a 64-bit BAR at the last slot;
- KD print lock/IRQL initialization and formatted-length issues in `src/kernel/kd/print.c`;
- pool free-list indexing in `src/kernel/mm/pool.c`;
- undefined return/shift-by-64 paths in ACPI field code;
- divide-by-zero in video scaling/printing;
- uninitialized paths in amd64 unwind and CRT scanning;
- possible null dereference in RT AVL removal/rotation logic; and
- unchecked boot-configuration numeric conversions.

Fixed-address page-map warnings and some packed/low-level conversions may be intentional. Every
suppression needs a local invariant and preferably a static assertion; mass `-fix` or a global
warning disable would erase useful signal. Recent history already contains targeted correctness
fixes in scheduler accounting, object/alert handling, partial pool expansion, PE validation, CRT
edge cases and AML references. [CODE] The audit should first lock those classes into tests.

There is no CTest, unit-test or other repository test suite. CI in `.github/workflows/amd64.yml`
builds Release with LLVM 21, creates FAT and ISO images and uploads them, but never executes either.
`run.sh` rebuilds images in the current build directory, mutates a persistent OVMF variable store,
uses KVM/host CPU and all host processors, and writes fixed names including `fat.img`,
`iso9660.iso`, `code.bin`, `vars.bin` and optionally `net0.pcap`. [CODE]

## 10. Core correctness audit passes

### Pass M — Physical, virtual and pool memory

#### Responsibilities and call paths

`MiInitializeEarlyPageAllocator` in `src/kernel/mm/page.c` consumes loader descriptors before the
full PFN database. `MiInitializePageAllocator` establishes `MiPageEntry` tracking and free lists.
`MmAllocateSinglePage`/`MmFreeSinglePage` use global state plus `KeProcessor.FreePageListHead` caches
at dispatch-level synchronization. `src/kernel/mm/pool.c` owns small block buckets and per-CPU
block caches. `poolpage.c` owns the pool virtual bitmap and mapped page caches; `map.c` pairs
`MmMapSpace`/`MmUnmapSpace` with architecture `HalpMap*` routines. `track.c` records pool tags.

#### Invariants to establish

1. Every managed physical page is in exactly one state and, if free/cached, exactly one list.
2. `total managed = reserved + allocated + global free + all per-CPU cached` under a defined
   snapshot protocol; every counter transition has a lock/IRQL owner.
3. A PFN’s flags, reference/use count, physical address and list membership agree.
4. Pool virtual bitmap ownership matches large allocation headers and mapped page count. Every
   small block belongs to one page/bucket/cache or one live allocation, never two.
5. Pool tag, requested size, actual bucket/page span and free path agree; double/foreign/misaligned
   frees are detected in diagnostic builds.
6. `base + length`, page rounding, count multiplication and bitmap indices are checked before use.
7. Partial page allocation/map expansion either commits fully or restores pages, PTEs, bitmap,
   counters and cache state exactly.
8. Map/unmap flags preserve cache/protection intent; TLB invalidation scope matches mapping scope;
   debugger temporary mapping is single-owner and unmaps on exception.

The existing TODOs that cached pool pages/blocks are not returned to higher-level allocators are
bounded-reclamation/fragmentation work, not automatically corruption. First instrument peak cache
sizes and pressure behavior; add reclamation only with a policy and stress evidence.

#### Instrumentation and assertions

- diagnostic page/pool headers with state, owner CPU, allocation sequence and tag;
- poison-on-free/allocate, optional redzones for small blocks and a small quarantine;
- a stop-the-world or lock-ordered MM snapshot that recomputes all accounting;
- list-cycle, duplicate-membership and bucket-index validation;
- injectable “fail next/Nth allocation or map” points;
- map/unmap and tag live-allocation counters exposed to debugger/headless diagnostics; and
- assertions documenting required maximum IRQL, interrupt state and held lock at internal entry.

Instrumentation must compile out or be explicitly configured for Release. It must not allocate
from the allocator it is validating while locks are held.

#### Stress/emulator scenarios

- deterministic randomized page/pool size/alignment/tag allocation/free on 1, 2 and 4 CPUs;
- simultaneous refill/drain of per-CPU caches and forced CPU reschedule;
- exhaust pool virtual space and physical pages at every partial expansion step;
- map/unmap unaligned physical ranges, maximum legal ranges and overflow/noncanonical ranges;
- repeated driver load failure and ACPI table/region mappings;
- debugger reads that fault mid-range and verify mapping cleanup; and
- long fragmentation run followed by a large contiguous/virtual allocation attempt.

Static analysis should prioritize checked arithmetic, array/bucket indices, null/error paths,
pointer provenance and acquire/release pairs. A code change is justified by a violated invariant,
an unhandled recoverable failure, a reproducible leak/corruption, or measured unbounded cache
pressure—not by stylistic preference.

### Pass E — Events, signals, mutexes and timers

`EvHeader` combines object type, spin lock, wait list and signaled state. `PsThread` embeds a matching
header used by wait/termination paths, so layout and ownership must remain synchronized. Signals
and recursive mutexes live in `src/kernel/ev`; `ev/dispatch.c` calls `PspSetupThreadWait` in
`src/kernel/ps/thread.c`, and `ps/scheduler.c` owns expiration-tree wakeups from the timer/DPC path.
[CODE]

**Invariants:** a thread has at most one active wait completion; signal, timeout, alert and
termination race to one winner; waiter list and timeout-tree membership agree with thread state;
auto/manual signal semantics match the declared object; mutex owner/recursion/contention are
consistent; only owner releases; synchronization-level/IRQL and lock acquisition order are stated;
timer conversion cannot overflow and expiration ordering is stable at equal deadlines.

**Instrumentation:** wait ID/generation, completion reason, object pointer, enqueue/dequeue CPU,
state-transition trace ring and contention/timeout counters. Assert the transition matrix and both
queue memberships while holding their documented locks.

**Stress:** signal just before/after timeout; signal/clear storms; multiple waiters; mutex recursion,
wrong-owner release and owner termination policy; alerts racing with wait/termination; timer wrap/
maximum duration; 1/4-CPU randomized schedules. Emulator tests need deterministic barriers/hooks
around race windows, not sleeps as proof.

Code changes require a reproducible double completion, stranded waiter, invalid owner/count,
overflow or a locally contradicted semantic contract. Do not redesign signal/mutex semantics to
match POSIX; Palladium must document its own executive behavior.

### Pass S — Scheduler and thread lifecycle

Per `KeProcessor`, runnable threads are on `ThreadQueue`, timed waits on `WaitTree`, termination
cleanup on `TerminationQueue`, and work on `WorkQueue`. `CurrentThread`, `IdleThread`, counts/ticks
and `PsThread` states coordinate through `src/kernel/ps/{thread,scheduler,alert,idle}.c`, the amd64
HAL context switch, timer DPC and HAL notifications/IPIs. Recent scheduler accounting fixes make
these transitions a first regression target. [CODE]

**Invariants:** define the allowed state matrix for Created, Queued, Running, Waiting, Suspended,
Terminated and Idle; exactly one running thread per online CPU; one queue/tree membership matching
state; idle never enters an ordinary termination/wait path; context `Busy`/ownership and saved stack
agree; thread/processor/global counts conserve; termination cleanup occurs only after no CPU can
return to the context; affinity and processor selection remain in range as CPUs come online.

**Instrumentation:** transition helper in diagnostic builds, per-thread last transition/reason/CPU,
run-queue verifier, context canaries and scheduler decision trace. Assertions must account for
transient states while the processor lock is held rather than sampling locklessly.

**Stress:** create/yield/delay/terminate loops, self/remote termination, suspended/waiting
termination, alert storms, processor work/IPI while switching, allocation failure during creation,
1/2/4 CPU and forced frequent ticks. Validate stacks/context under exception/unwind stress.

Static analysis should focus on list unlink/insert pairing, stale `CurrentThread`, signed/width time
math, lock/IRQL exits and noreturn assumptions. Change code only where the state specification and
trace demonstrate an illegal transition/lifetime or error rollback is incomplete.

### Pass O — Objects and ownership

`src/kernel/ob/object.c` places an `ObHeader` before the body with type, parent/directory entry,
reference count and tag. `directory.c` owns root and member insertion/removal/lookups. Directory
membership takes a reference; current lookup-by-name/index returns a body pointer after releasing
the directory lock, so the borrowed-versus-owned contract is a high-priority lifetime question.
[CODE]

**Invariants:** body/header conversion and alignment; count never zero-to-live, underflows or
overflows; creation starts with a documented reference; directory membership owns exactly one;
remove atomically unlinks/clears parent and releases membership; lookup returns either a referenced
object or a borrow whose enclosing lock/lifetime is explicit; destructor runs exactly once outside
unsafe locks; directory parent cycles and duplicate names are rejected according to policy.

**Instrumentation/tests:** reference history ring in diagnostic builds, object/tag live registry,
forced count boundaries, concurrent lookup/remove/reference/dereference, recursive directory
destruction, duplicate insertion and allocation failure. Object tests should use fake types with a
destructor counter before exercising threads/drivers.

A code change is justified whenever a public exported lookup cannot state how its result remains
alive, or a race can reach freed memory/double destruction. Do not introduce handles, ACLs or a
full NT object namespace merely to fix reference ownership.

### Pass C — CRT

`src/sdk/crt/{ctype,math,stdio,stdlib,string}` provides ctype, limited math, formatting/scanning,
conversions and strings; boot and kernel select freestanding subsets, while the OS/user portions
add allocation, file/locking and startup pieces. Public headers and `src/sdk/crt/ucrt.def` are the
declared ABI. The `.def`
currently duplicates `getchar`, `gets`, `putchar` and `puts`; header breadth is wider than the
implemented/exported profile. [CODE]

**Invariants:** each target-kind header declaration is either implemented/exported or explicitly
outside its profile; size/count arithmetic cannot wrap; C standard return/termination/overlap rules
hold; `errno`/floating behavior is either conforming or documented unsupported; formatter/scanner
argument type and consumed-input state remain consistent on failure; allocation and FILE locking
pairs close on all exits.

**Tests:** host differential cases against the C23 contract for defined behavior, not libc internal
behavior; boundary vectors for every string/memory numeric function; format/scan table covering
width, precision, length modifiers, invalid input, EOF and truncation; fuzz format/input under
sanitizers; compile/link tests per boot/kernel/user export list. Specifically reproduce scanner
pointer-subtraction warnings, `fflush(NULL)`, NaN/inf, base conversion overflow and duplicate
exports.

Change implementation when the pinned standard, declared Palladium profile or a memory-safety test
is violated. It is acceptable to declare wide stdio/math outside the profile. Do not broaden the
freestanding ABI as part of a correctness fix.

### Pass R — RT, exception and unwind

The architecture-neutral files at the root of `src/sdk/rt` implement intrusive D/S lists, bitmap,
AVL tree and hash. Kernel RT adds atomic SList plus amd64 context save/restore, PE image function
lookup, `RtVirtualUnwind`, `RtUnwind`,
`RtDispatchException` and `__C_specific_handler`; these are exported by `kernel.def`, while `urt.def`
exports the portable subset plus context calls for user RT. [CODE]

**Invariants:** intrusive nodes are in at most one list/tree; head/tail/size/height/balance agree;
bitmap ranges are contained and find operations cannot wrap; atomic SList header is correctly
aligned and ABA/depth/sequence semantics are stated; PE runtime-function lookup validates sorted
ranges; unwind codes are bounded by record/prologue/frame/stack; each unwind step makes valid
progress; exception dispositions and collided/nested unwind states follow the selected Microsoft
ABI.

**Tests:** property/model tests for lists, bitmaps and AVL insert/remove/index/enumeration; random
operation traces and corruption assertions; concurrency stress for atomic SList; compiler-generated
amd64 functions with known prolog/epilog/unwind records; synthetic malformed/chained records;
exceptions through C/assembly frames and fatal stack trace. Cover the currently noted missing
unwind-v2 `UWOP_EPILOGUE` path and static analyzer uninitialized path. ARM64 unwind is a new ABI
implementation, not a parameter to the amd64 decoder.

The [Microsoft x64 exception-handling documentation](https://learn.microsoft.com/en-us/cpp/build/exception-handling-x64)
and PE format are normative for the current implementation. [SPEC] Change code for a violated data-
structure invariant, malformed-record unsafe access, or ABI vector mismatch. Do not rewrite
intrusive structures into libc containers.

## Detailed epic plans

### Epic V1: Build, image and QEMU automation

**Objective / done.** One documented command builds relevant targets and runs a bounded amd64 smoke
profile whose pass/fail is machine-readable. Image construction is independent from execution;
each run uses disposable firmware variables/output names; proprietary KD modules are optional,
external inputs and never CI artifacts. CI preserves Release/LLVM 21 and adds host tests plus QEMU
when the runner supports it.

**Source.** `src/CMakeLists.txt`, SDK CMake helpers, `.github/workflows/amd64.yml`, `run.sh` (or small
replacement scripts under a tooling directory), README; new host-test targets and QEMU expected-
output specification. Generated images/logs remain ignored.

**Architectural decisions.** diagnostic channel independent of KD (serial, debug port or another
Palladium sink), fixed QEMU machine/CPU/memory/SMP profiles, acceleration fallback, fresh OVMF vars,
timeout/exit mechanism, artifact paths, split public versus local proprietary-debug lanes. A serial
sink is a HAL/diagnostic backend, not a Unix TTY design.

**Phases / commits.** (1) boot-marker contract. (2) pure image builder with parameters and no QEMU.
(3) disposable QEMU runner/captured diagnostics. (4) log assertion/timeout. (5) host test CMake
entry. (6) CI build+host tests. (7) optional CI QEMU or documented local gate. (8) local KD lane.

**Tests first.** script dry-run/argument/error cases; missing tools/firmware; success and forced
loader/kernel/ACPI failure logs; repeated fresh-variable runs; 1 and 4 CPUs; Debug/Release build
matrix where affordable. **Risks:** hosted runner lacks KVM, TCG timing, firmware package drift,
false-positive text marker, script overwrites. Depends on no feature epic and should land first.
Do not mix it with debugger protocol or kernel correctness fixes. **Model:** Sol/medium automation,
Sol/high diagnostic boundary; Luna/xhigh test review. **Subagents:** helpful for CI versus local
runner after output contract freezes and files are separately owned.

### Epic C1: Memory audit

**Objective / done.** Pass M equations and ownership are documented in code-facing design notes;
diagnostic verification and fault injection pass deterministic 1/2/4-CPU stress; every discovered
defect has a focused regression and cleanup/accounting is checked after failure.

**Source.** `src/kernel/mm/*`, amd64 `hal/map.c`, `KeProcessor` cache fields, loader memory
descriptors and KD temporary mapping only where they consume MM contracts.

**Decisions.** snapshot/lock ordering, state enum and counter definitions, diagnostic header costs,
cache high-water/reclamation policy, mapping transaction boundary, allowed allocation/IRQL per API.

**Phases.** (1) equations and transition inventory. (2) verifier without behavior changes. (3)
fault-injection hooks. (4) page/PFN tests. (5) pool/bitmap/cache tests. (6) map rollback/TLB tests.
(7) fix one defect per regression. (8) measured reclamation decision.

**Tests first.** partial allocation/map failure at every step; duplicate/foreign free; max/overflow
ranges; SMP refill/drain; long fragmentation. **Risks:** verifier reentrancy, false assertions on
legal transient state, perturbing race timing, lock inversion. Depends on V1 for emulator evidence.
Commit instrumentation separately from fixes and reclamation. Do not combine with HAL abstraction
or ARM64 mapper. **Model:** Sol/xhigh for concurrency/transaction design, Sol/high fixes; Luna/xhigh
stress/model review. **Subagents:** helpful for independent PFN, pool and map inventories after one
owner freezes equations; overlapping MM implementation should stay serial.

### Epic C2: Events, threads, scheduler and objects

**Objective / done.** Passes E/S/O have written transition and ownership contracts, diagnostic
traces/verifiers and deterministic race tests. Signal/timeout/alert/termination completes a wait
once; scheduler queues/counts conserve; public object lookup lifetime is unambiguous and tested.

**Source.** `src/kernel/{ev,ps,ob,ke/{ipi,work,lock}.c}`, common HAL notification/timer calls,
related public/private headers and `kernel.def` if ownership semantics require a compatible API
addition.

**Decisions.** lock order and IRQL, wait winner protocol, mutex owner-termination policy, scheduler
transition matrix, termination reclamation grace, borrowed versus referenced directory lookup.

**Phases.** (1) event/thread/object contract notes and trace IDs. (2) signal/mutex wait tests. (3)
alert/timeout tests. (4) scheduler queue verifier and lifecycle stress. (5) object reference tests.
(6) concurrent directory removal. (7) focused fixes. Keep object changes separate from scheduler
changes even within the program.

**Tests first.** deterministic race hooks around enqueue/completion/unlink; create/terminate loops;
wrong-owner mutex release; allocation failure; object destructor counters. **Risks:** instrumentation
changes timing, debug trace allocation under lock, correcting borrowed ownership breaks callers,
deadlocks hidden by single CPU. Depends on V1 and benefits from C1 fault injection. Do not combine
with debugger SMP rendezvous; land its required generic CPU-stop primitive only after scheduler
rules are known. **Model:** Sol/xhigh concurrency design/fixes; Luna/xhigh state-machine tests and
independent caller audit. **Subagents:** useful with explicit EV, PS and OB file ownership after the
shared lock/transition contract freezes; integration remains one owner.

### Epic C3: CRT and RT audit

**Objective / done.** Declared profiles/exports are consistent; defined C23 behaviors have boundary
and sanitizer tests; RT structures pass property/concurrency tests; amd64 exception/unwind passes
compiler-generated and malformed-record vectors; known high-signal tidy findings are resolved or
locally justified.

**Source.** `src/sdk/crt`, `src/sdk/rt`, target-specific CMake and `.def` files; kernel exception/
fatal consumers and paired amd64 context headers/assembly.

**Decisions.** explicit boot/kernel/user CRT profile, host test adapter versus production code,
supported format/scan matrix, atomic SList memory-order/ABA contract, unwind ABI version coverage,
malformed-image error policy.

**Phases.** (1) declaration/definition/export matrix. (2) memory/string/numeric tests. (3) format/
scan fuzz and fixes. (4) list/bitmap/AVL properties. (5) atomic SList stress. (6) amd64 unwind vector
generator and exception tests. (7) tidy warning classification. (8) document intentionally absent
surface.

**Tests first.** duplicate export/link check; sizes 0/1/max and overlap; invalid format/input;
random AVL reference model; malformed unwind truncation and generated prologs. **Risks:** host libc
or compiler builtins replace functions under test, undefined C behavior used as oracle, unwind test
compiler version drift, changing exported ABI. Depends on V1 host-test support; unwind benefits from
debugger contexts but does not wait for breakpoints. Commit CRT, portable RT, atomics and unwind
separately. Do not combine ARM64 unwind with amd64 corrections. **Model:** Sol/high, Sol/xhigh for
unwind/exception; Luna/xhigh vectors/property review. **Subagents:** well suited because CRT,
portable RT and unwind have disjoint ownership after common test infrastructure is fixed.

## Static-analysis program

1. Preserve a machine-readable baseline by tool version/configuration. Do not gate on the raw
   historical count.
2. Classify checks into security/correctness, suspicious portability, deliberate low-level pattern
   and noise. Gate **new** high-signal diagnostics first.
3. Reproduce analyzer findings with a focused unit/emulator case before changing tricky low-level
   code. Add a local `NOLINT` only with the invariant it relies on.
4. Tackle warning families subsystem-by-subsystem so paired architecture/ABI code is reviewed
   together. Macro-parentheses may be mechanically cleaned only in project-owned macros and a
   separate change; do not touch EDK2-style headers.
5. Run the documented `run-clang-tidy -p ... -source-filter='.*\.(c)$'`; never apply broad `-fix`.

Clang-tidy is not proof of runtime correctness, but its current high-signal findings overlap the
roadmap’s most sensitive boundaries: malformed loader input, KD packet/device handling, allocator
indices, AML field arithmetic, intrusive structures, formatting/scanning and unwind.

## Cross-cutting ABI and security gate

For every epic, review:

- public headers, `.def` exports and all callers before shared signature changes;
- `OslpBootBlock`/`KiLoaderBlock`, debugger packets, PE structures and context layouts for packing,
  explicit widths, version and static layout assertions;
- external lengths/counts/addresses with checked add/multiply/rounding before allocation/access;
- matching allocate/free, map/unmap, acquire/release, packet get/release and reference/dereference
  on every exit, preserving the four-byte pool tag;
- current IRQL, interrupt state, lock ownership/order, reentrancy and allowed allocation;
- expected malformed/exhaustion paths as recoverable status, reserving fatal/assertions for proven
  internal invariants; and
- diagnostics that reveal subsystem/phase/reason without leaking proprietary artifacts or host
  machine paths.

What should deliberately remain unchanged: PE/UEFI orientation, custom MM/executive/CRT/RT, explicit
public/private split, architecture mechanisms under architecture paths, boot-driver status of ACPI,
and the absence of speculative Unix process/file/device abstractions. A future driver model needs a
separate requirements RFC derived from a second concrete driver lifecycle, not a correctness-audit
side effect.
