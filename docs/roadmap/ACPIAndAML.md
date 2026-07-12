# Custom ACPI/AML completion plan

Evidence labels are defined in [the roadmap index](README.md). The project’s custom implementation
is a design constraint and a useful learning/engineering asset. ACPICA, uACPI and `iasl`/`acpiexec`
may supply behavioral comparisons and test generation; they are not replacement proposals.

The normative baseline should be pinned per test corpus. The current public specification is
[ACPI 6.6](https://uefi.org/specs/ACPI/6.6/), especially Chapter 5 (tables), Chapter 19 (ASL) and
Chapter 20 (AML). [SPEC] Where firmware was compiled for an older ACPI revision, tests must state
the applicable revision rather than silently applying the newest behavior.

## 9. Current responsibilities and boundaries

- `src/drivers/acpi/main.c` is driver entry and namespace/device initialization. It locates DSDT/
  SSDT content, populates the tree and performs `_STA`, `_INI` and `_REG`-related initialization.
- `table.c` and `os/table.c` obtain, map, cache and validate firmware tables through kernel HAL
  services. Table mapping lifetime and checksum/length validation are security boundaries.
- `interp/args.c` is the parser’s opcode argument grammar: 256 primary and 256 extended slots mark
  recognized opcodes, fixed arguments and package-length bodies.
- `opcode/opcode.c` drives iterative argument parsing and dispatches opcode families in
  `opcode/{concat,conv,dataobj,field,lock,math,namedobj,nsmod,ref,stmt,target}.c`.
- `interp/aml.c` owns namespace population/evaluation, `AcpiValue` reference/copy/cast behavior,
  method invocation state and public `AcpiExecuteMethod`/`AcpiEvaluateObject` paths.
- `interp/field.c` implements field-unit reads/writes, update rules, BankField/IndexField and region
  routing. `interp/object.c` owns name resolution and object-tree operations, while `interp/aml.c`
  owns namespace population and value/object lifetime helpers.
- `os/{mmio,port,pci}.c`, `region/{ec,cmos}.c` and the other OS adapters connect operation regions
  to HAL/kernel services. In the current tree the functional set is SystemMemory, SystemIO, PCI
  configuration, EmbeddedControl and CMOS; `os/pci.c` still contains PCI segment limitations.
  [CODE]
- Public interfaces under `include/public` and `acpi.def` expose search/path, evaluate/execute,
  casts, copies and references to other Palladium components.

### Important structures and invariants

`AcpipState` holds the current scope/opcode, arguments, locals and method state. `AcpipScope`
defines a bounded AML byte span; `AcpipOpcode` forms a heap-allocated parent chain while nested term
arguments are evaluated. `AcpiObject` forms the namespace and stores an `AcpiValue`; values include
integers, strings, buffers, packages, methods, references, fields, mutexes/events and named device
classes. [CODE]

The intended invariants are:

- every read advances only within the current `RemainingLength` and every package is contained in
  its parent package/table;
- every owned `AcpiValue` reference is released exactly once on success and every failure edge;
- namespace definitions have one stable parent/name identity; aliases and references do not create
  accidental ownership cycles;
- method locals/arguments/temporaries cannot outlive their backing objects incorrectly;
- a field access is contained in its buffer/region and applies access width, lock and update rule
  exactly once;
- mutex acquisition obeys sync-level ordering, timeout and recursion semantics;
- interpreter errors are recoverable data errors until the boot-driver policy deliberately decides
  a required firmware contract is fatal.

Several of these exist only as local code assumptions. They need executable checks before broad
opcode expansion.

## AML operation and semantic inventory

### Implemented families requiring tests, not reimplementation

The current handlers cover the following substantial subset [CODE]:

- constants and data: Zero/One/Ones, byte/word/dword/qword/string prefixes, Buffer, Package,
  VarPackage, Revision and Timer;
- namespace/named objects: Alias, Name, Scope, Method, Event, OperationRegion, Device,
  ThermalZone, Processor and PowerResource;
- methods and references: Arg0–Arg6, Local0–Local7, Store, RefOf, DerefOf, Index, CondRefOf,
  SizeOf, ObjectType and CopyObject; invocation of method and non-method names;
- arithmetic/bitwise/logical: Add, Subtract, Multiply, Divide, Mod, Increment/Decrement, shifts,
  And/Nand/Or/Nor/Xor/Not, FindSetLeft/RightBit, logical And/Or/Not and comparisons;
- conversion/composition: Concat, Mid, ToBuffer, ToDecimalString, ToHexString, ToInteger,
  ToString, FromBCD and ToBCD;
- fields: CreateBit/Byte/Word/DWord/QWordField, Field, IndexField and BankField;
- flow/control: If/Else, While, Break, Continue, Return, Noop, BreakPoint, Stall, Sleep, Match,
  Fatal and Debug object;
- synchronization: Mutex, Acquire, Release, Event, Wait, Signal and Reset.

“Implemented” means a handler exists, not that all specification semantics are proven. High-value
verification targets include package/reference copying, Store conversion rules, divide-by-zero and
shift counts, String/Buffer conversion termination, Index bounds, method serialization/sync level,
While/Break/Continue nesting, BankField selector ordering, field lock/update rules, Acquire timeout,
and Fatal policy.

### Known partial, recognized-but-unhandled and absent areas

- `Notify` is parsed and only logs a trace; there is no notification registration/dispatch/lifetime
  contract. [CODE]
- ConnectField is explicitly unimplemented in field-list parsing and reaches fatal error policy.
  [CODE]
- `Load`, `LoadTable`, `DataRegion` and `External` appear in the valid argument grammar but have no
  complete execution/namespace handler in the dispatch set. They must be marked explicitly in the
  mechanical report instead of being discovered through “unimplemented opcode” fatal behavior.
  [CODE]
- Dynamic table load/unload semantics, namespace collision/rollback and references to unloaded
  objects are therefore absent or incomplete even if an `Unload` token is recognized. [CODE]
- SMBus, PCI BAR target, IPMI, GPIO, GenericSerialBus and PCC operation-region implementations are
  absent. SystemIO is not universally meaningful on ARM64. [CODE]
- PCI configuration has segment/ECAM limitations; operation-region connection ordering and `_REG`
  behavior need revision-pinned tests. [CODE]
- Resource-template concatenation/checksum behavior and less-common field access forms need a
  handler-by-handler audit even where `ConcatRes` is recognized. [QUESTION]
- There is no public Notify consumer model, handler removal model or driver-device lifetime model;
  implementing the opcode before that policy would create an orphan callback mechanism. [CODE]

Priority must come from the intersection of: required by ACPI initialization, observed in a legally
redistributable/generated table corpus, security relevance, and dependency on missing object/
notification infrastructure. A rarely used opcode is not higher priority merely because its slot
is valid.

## Execution-state, lifetime and malformed-input risks

The opcode-parent chain and scope traversal are mostly iterative, which is a good existing design.
Method invocation in `opcode.c` calls `AcpiExecuteMethod` recursively, and namespace child
initialization recursively walks objects. No explicit method-call depth, namespace depth, opcode
nesting depth or execution/fuel budget is visible. A valid or malicious recursive method/While can
exhaust the kernel stack or monopolize boot. [CODE]

**[REC]** Add four independent limits with typed errors:

1. parser/package nesting depth;
2. namespace traversal depth/object count;
3. method call depth and total live execution frames;
4. per-evaluation instruction/loop budget, optionally with elapsed-time diagnostics but not timing
   as the correctness mechanism.

Do not use one “recursion limit” for all four. Limits need defaults, test overrides, peak counters
and error paths that unwind scopes/opcodes/locals/args/references. Convert recursive namespace
initialization to an explicit work stack if generated-depth tests show a realistic problem; do not
rewrite it preemptively solely for style.

Malformed AML currently has paths where a parse/dispatch failure becomes
`AcpipShowErrorMessage(ACPI_REASON_CORRUPTED_TABLES, ...)`, which is appropriate only after the
driver has decided the table is boot-critical. [CODE] The interpreter core should instead return a
typed diagnostic containing table signature/OEM identity, AML byte offset, opcode, namespace path,
phase and error code. The kernel adapter decides whether to disable one device/table, reject a
method, or halt initialization.

Integer behavior is `uint64_t` throughout. Verify revision-dependent integer width and conversion/
overflow rules against the selected ACPI revision, including shifts by 64 or more—the current
field code triggered a static-analysis warning for such a shift. Buffer/package lengths, bit
offset+length, region base+length and allocation multiplication must use checked arithmetic before
pointer or region access.

Reference counts appear non-atomic. That can be acceptable while namespace construction/evaluation
is serialized during boot, but mutex/event support and future notification/device work make the
execution concurrency contract a required decision. [QUESTION] First document whether concurrent
`AcpiEvaluateObject` calls are forbidden, globally serialized, or supported; do not “fix” it by
making only the count atomic while object contents/namespace remain unprotected.

## Test architecture

### Hosted interpreter core

**[REC]** Build the parser, namespace, value and opcode-family code as a hosted test target with an
`AcpipOsOps`-style adapter for allocation/free, diagnostics, timer/sleep, mutex/event, table access
and operation-region I/O. This is a build personality, not a second interpreter. Kernel-specific
files under `os/` remain the production adapter.

The host adapter provides:

- tracked allocator with fail-on-Nth-allocation, poison and leak/reference balance reports;
- deterministic virtual time for Stall/Sleep/Timer/Acquire/Wait;
- fake byte-addressable SystemMemory/SystemIO/PCI/EC/CMOS regions with permission and bounds logs;
- nonfatal structured diagnostics and optional trace;
- deterministic synchronization for single-thread tests, then explicitly scheduled multi-thread
  tests if concurrency is declared supported; and
- a namespace/result serializer used only by tests.

### Corpus and oracle layers

1. **Hand-encoded minimal AML** tests byte-level parser boundaries and every prefix/package-length
   encoding.
2. **ASL source compiled by `iasl`** exercises readable semantic cases. Pin tool version and keep
   source plus provenance; generated AML may be regenerated in CI or stored only after licensing/
   reproducibility review.
3. **Synthetic complete ACPI tables** test checksum, length, DSDT/SSDT order, duplicate definitions
   and revision policy.
4. **Legally redistributable firmware tables** expand realistic coverage. Never commit tables
   whose redistribution rights are unclear; store hashes/coverage metadata for local-only corpora.
5. **Mutation/fuzz tests** truncate at every byte, perturb package lengths/names/opcodes/counts and
   run libFuzzer/AFL-style entry points under ASan/UBSan in the hosted build.
6. **Differential comparison** may run the same defined ASL with ACPICA `acpiexec` or uACPI and
   compare namespace/value/region traces. A mismatch is a research item resolved against ACPI, not
   automatic proof that Palladium is wrong. Undefined, implementation-defined or revision-sensitive
   cases are excluded or annotated.

The first generated report must join the valid entries in `interp/args.c` to all handler cases and
produce: implemented/tested, implemented/untested, partial, deliberately rejected, or accidental
fallthrough. Track semantic dimensions separately; one happy-path opcode test is not full coverage.

### Required suites

- namespace paths, parents, aliases, duplicate/name collision, DSDT/SSDT overlay and External;
- object create/copy/reference/remove, nested packages, Store to Arg/Local/field/reference, failure
  cleanup and reference cycles;
- method arity, nested invocation, serialized methods, sync levels, return/implicit return and all
  locals/args released;
- If/Else/While/Break/Continue and execution-budget exhaustion;
- every conversion across integer/string/buffer plus boundary/overflow and revision width;
- buffer/package/Index/Mid/CreateField bounds and malformed package counts;
- region and Field/BankField/IndexField access traces for each access/update/lock rule;
- mutex recursion/order/timeout and event wait/signal/reset;
- depth limits, allocator failure at every allocation site, truncated AML and unknown opcode;
- table checksum/length/address overflow and map/unmap pairing; and
- `_STA`, `_INI`, `_REG` ordering and error containment on generated device trees.

## Detailed epic plans

### Epic A1: Hosted AML test harness

**Objective / done.** The production interpreter core compiles in a host test target with no kernel
boot. Minimal tables construct/query a namespace and execute methods/fields; allocation, region and
diagnostic behavior are injectable; recognized/handled coverage is generated; sanitizers and
malformed corpus run in one documented command.

**Source.** `src/drivers/acpi/{interp,opcode}`, selected table/object code and public/private headers;
new host adapter and CMake test target; generated/handwritten test data outside build artifacts.
Production `os/*` behavior remains unchanged until a test demonstrates a defect.

**Architectural decisions.** Narrow OS-ops boundary, structured error type and byte-offset tracking,
test-only result serializer, fixture provenance, whether `iasl` is generation-time or checked CI
dependency. Do not expose host testing hooks in `acpi.def`.

**Phases / reviewable tasks.** (1) compile one pure value/name module with host CRT. (2) tracked
allocator/diagnostics. (3) parser/namespace minimal table. (4) method evaluation. (5) fake regions
and virtual time. (6) mechanical opcode report. (7) truncation/fault injection. (8) sanitizer fuzz
entry. Each phase adds tests before refactoring further dependencies.

**Tests first.** One constant Name, one method returning arithmetic, one bounded region field, a
truncated package, allocation failure and a recursive method hitting a temporary test limit.

**Risks/failures.** Accidentally forking production logic; host libc hiding freestanding behavior;
fatal policy inseparable from parser; nondeterministic timers; fixture licensing. Depends on build
test infrastructure but not debugger work. Commit adapter boundaries separately from semantic
fixes. Do not implement missing opcodes in this epic. **Model:** Sol/high for isolation/error model,
Sol/medium for fixtures; Luna/xhigh for corpus/report/fuzz review. **Subagents:** helpful for fixture
generation and opcode-inventory tooling after one owner defines the OS-ops/error boundary.

### Epic A2: Missing AML opcode and semantic wave

**Objective / done.** No recognized opcode accidentally reaches generic fatal fallthrough. Notify
has a defined consumer/lifetime model or is explicitly rejected without side effects; the first
corpus-justified missing operations are conformant and tested; unsupported regions/operations fail
with typed diagnostics; depth/fuel and malformed-input policies are enforced.

**Source.** `interp/args.c`, dispatcher/opcode families, namespace/object/value/field/method files,
operation regions, driver initialization and public API only if Notify consumers require it.

**Decisions.** Notify registration/removal/dispatch IRQL and ownership; dynamic table load/unload
transaction model; External namespace semantics; DataRegion source validation; ConnectField
behavior; execution/depth budgets; concurrency policy; per-op boot-fatal containment.

**Prioritized phases.** (1) explicit status for every recognized-but-unhandled opcode. (2) depth/
fuel limits and complete unwind cleanup. (3) Notify mechanism with no consumers first, then one
real consumer. (4) External/DataRegion if corpus needs them. (5) Load/LoadTable/Unload only as one
transactional namespace project. (6) ConnectField and missing operation regions only from concrete
firmware demand. (7) field/reference/conversion bug wave found by harness/fuzzing.

**Tests first.** Golden spec cases per operation, failure rollback snapshots, reference/allocator
balance, nested methods and loops at/beyond limits, malformed package/table, concurrent evaluation
only if supported. Differential traces are supporting evidence; expected values cite ACPI clauses.

**Risks.** Dynamic namespace dangling references, Notify callbacks after object removal, mutex
deadlock, region side effects during validation, a “completeness” wave too large to review. Depends
on A1 and C2 object/event ownership rules. Commit each semantic family separately with spec-linked
tests. Do not combine Load/Unload with Notify, field rewrite, or ARM64 region work. **Model:** Sol/
xhigh for method/reference/field/synchronization semantics; Sol/high for bounded opcode work; Luna/
xhigh independent vectors. **Subagents:** useful by opcode family only when object ownership APIs
are frozen and files do not overlap; one integrator owns namespace/reference semantics.

## What should remain unchanged

- Keep the custom parser, namespace, interpreter and operation-region model.
- Keep ACPI as a boot driver rather than moving all AML into the HAL; the HAL should expose only
  early platform-table and hardware primitives.
- Keep iterative opcode/scope execution where it already avoids C recursion.
- Keep explicit value types/reference operations and public/private header separation.
- Do not add every ACPI operation region speculatively, and do not treat another implementation’s
  internal object model as Palladium’s architecture.
- Do not turn expected malformed firmware into assertions. Assertions remain for interpreter
  invariants whose violation proves a Palladium bug; typed errors represent bad external data,
  unsupported semantics and allocation failure.
