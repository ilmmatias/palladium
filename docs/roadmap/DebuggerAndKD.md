# Debugger, protocol, breakpoints, extensions and transports

Evidence labels are defined in [the roadmap index](README.md). This plan preserves Palladium’s
custom debugger protocol and its legally supplied KD extension adapter; it does not claim wire
compatibility with Microsoft KD.

## 7. Current debugger design analysis

### Target-side boundaries and call paths

`KdpInitializeDebugger` in `src/kernel/kd/initialize.c` is called during boot-processor
initialization. It consumes `KiLoaderBlock.Debug`, calls `KdpInitializeDeviceDescriptor`, builds the
WDK-shaped import/export tables in `import.c` and `export.c`, invokes the external module’s
`KdInitializeLibrary`, allocates its hardware context, initializes the controller, and enters
`KdpEnterReceiveLoop(KDP_STATE_EARLY)` until the host connects. [CODE]

The present layers are only partially separated:

- `src/kernel/kd/device.c` translates the loader’s PCI BDF into `KdpDebugDeviceDescriptor` and
  reads BARs. It assumes a PCI network controller; the 64-bit BAR path needs an explicit bound when
  the low BAR is index 5.
- `src/kernel/kd/import.c` and `export.c` adapt the external module’s `KdNet` API. Several imports
  log “unsupported”, including hibernation, VMBus and hypervisor helpers. Compatibility therefore
  depends on which imports a module actually exercises, not merely whether it loads.
- `src/kernel/kd/ethernet.c`, `arp.c`, `ip.c` and `udp.c` implement Palladium’s current packet
  path. Controller polling, extension buffer acquisition/release, network framing and debugger
  protocol service are joined by global state.
- `src/kernel/kd/protocol.c` decodes one-byte request types into connect, physical/virtual memory
  reads and amd64 port reads. It uses a 1024-byte response buffer. It checks several lengths but
  lacks exact envelope length, overflow-safe range helpers, session/correlation identifiers and
  structured error replies. An exception during mapped virtual-memory access can bypass the
  matching unmap path.
- `src/kernel/kd/print.c` formats into a static buffer and emits print packets. The `vsnprintf`
  return value must be clamped before it becomes a wire length; lock/IRQL state must be valid on
  both locked and early/unlocked paths.
- `KdpEnterReceiveLoop` is a global polling state machine. The late `KDP_STATE_BREAK` path has no
  transition that returns to execution. Panic CPU freezing in `src/kernel/hal/amd64/smp.c` is
  one-way and is not a debugger rendezvous.

### Host-side coupling

`src/debugger/main.py` initializes curses before connecting and runs a 10-ms polling loop.
`connection.py` owns handshake and interface shutdown. `receiver.py` decodes packets and calls
presentation helpers directly. `command.py` parses input, sends raw socket datagrams and writes the
single global `protocol.KdpCurrentState`; the receiver uses that global to guess how a reply should
be decoded. `interface.py` owns terminal buffers, log and command line; `utils.py` also performs
presentation-aware disassembly. [CODE]

Consequences:

- only one uncorrelated request may be outstanding;
- an unexpected ACK is not rejected in every path, and an empty/malformed datagram can escape as a
  Python unpack/decode exception;
- the source tuple returned by `recvfrom` is not consistently part of reply validation;
- headless operation still imports/initializes curses;
- protocol actions cannot be unit-tested without simulating UI globals;
- register request constants exist in the client without a matching target handler, which is an
  ABI-development anti-pattern;
- Capstone is a runtime dependency but the README’s dependency statement does not declare it; and
- “break” is a UI state, not a target lifecycle with stop reason, CPU context and resume policy.

### Recommended internal design

```text
TUI frontend ---------\
                       -> Command API -> DebugSession -> Codec -> DatagramTransport
Headless/JSON frontend-/                    |
                                            +-> typed events -> output sink(s)
Fake target/test transport -----------------+
```

**[REC]** Keep one protocol implementation:

1. `DatagramTransport` only sends/receives byte strings, records peer identity, and implements
   bounded deadlines. A fake/in-memory transport implements the same narrow interface.
2. A pure `Codec` converts typed packet objects to/from bytes with no socket, UI or global state.
3. `DebugSession` owns negotiation, session ID, sequence/request correlation, pending requests,
   target running/stopped state, stop context and reconnect policy.
4. A typed `CommandService` exposes `read_memory`, `registers`, `breakpoint_add`, `continue`, `step`
   and similar operations. It never renders and never sends raw packet constants.
5. Frontends translate input to the command service and typed events to curses, stable text, or
   JSONL. Headless scripts get deterministic exit codes, per-command timeout and an option to fail
   on target disconnect.

Do not introduce a second “headless protocol.” Do not make the target parse textual commands. Do
not use asyncio merely for fashion: a selector/thread or synchronous request loop is sufficient
until multiple simultaneous operations are a demonstrated need.

## Protocol v1 recommendation

The current one-byte format should be named **v0** and frozen. [REC] Define a v1 envelope with:

- magic, protocol major/minor, header size, message type and flags;
- total/payload length with a negotiated maximum;
- session ID, monotonically increasing sequence and request/reply correlation ID;
- a status code in every reply and a capability bitmap in negotiation;
- explicit little-endian integer encoding and exact-length rejection;
- architecture ID and versioned context schema, with a valid-register bitmap;
- stop event containing reason, CPU ID, architecture context version and relevant address/code;
- distinct asynchronous events versus replies; and
- optional integrity/authentication capability reserved, not falsely implied by UDP checksum.

An initial v1 need not encrypt or authenticate. It must state that debugging is a trusted-network
boot feature and reject packets from a non-negotiated peer/session. Sequence windows should reject
stale/replayed state-changing requests while permitting a duplicate idempotent reply for a retried
read. Breakpoint writes, continue and step require at-most-once request semantics. Invalid messages
receive a bounded error only after peer/session validation to avoid amplification.

Migration should be negotiation-first: a client sends a small discovery datagram distinguishable
from v0; a v0 target remains usable for memory reads during a short transition. Do not add new v0
commands, and delete compatibility at the milestone named in the roadmap’s blocking question.

## amd64 breakpoint and execution-control design

### Stop ownership and SMP

**[REC]** Add a target `KdpStopController`, not a special case in packet dispatch. It owns:

- running, stopping, stopped and resuming transitions;
- debugger-owner CPU and per-CPU captured context/status;
- a reversible SMP rendezvous separate from `HalpFreezeProcessors` panic behavior;
- the breakpoint table and any pending internal step-over; and
- debugger disconnect/reconnect policy.

The exception dispatcher in `src/kernel/hal/amd64/idt.c` must offer `#BP` and `#DB` to the debugger
only when the exception matches debugger-owned state. Unclaimed exceptions continue through the
normal `RtDispatchException`/fatal policy. A random breakpoint instruction must not be silently
consumed because a debugger once connected.

On a stop, one CPU becomes owner, raises to a documented level with interrupts in a documented
state, requests the other online CPUs enter a safe quiescent rendezvous, and records which CPUs
acknowledged. Timeouts are explicit; partial quiescence is a stop failure, not permission to patch
or inspect mutable shared state. Normal kernel synchronization cannot be assumed usable if the
stopped CPU owns its lock.

### Software breakpoint lifecycle

For amd64, an initial software breakpoint replaces one byte with `INT3` (`0xCC`). Intel’s system
programming manuals are the architecture authority for breakpoint/debug exceptions
([Intel SDM landing page](https://www.intel.com/content/www/us/en/developer/articles/technical/intel-sdm.html)).
[SPEC] Microsoft’s public debugger documentation also describes software breakpoints as instruction
replacement and notes that a kernel software breakpoint applies across processors
([breakpoint methods](https://learn.microsoft.com/en-us/windows-hardware/drivers/debugger/methods-of-controlling-breakpoints)).
[SPEC]

The target, not the client, owns a fixed/bounded breakpoint table containing address, address-space
identity, saved original byte, enabled/disabled state, hit count and generation. Insert/remove is
atomic with respect to the debugger rendezvous. The patch helper validates mapping and executable
policy, writes through an architecture hook, enforces instruction/data cache and ordering rules,
and returns a structured status. x86 has coherent instruction/data caches, but ordering and
serialization around self-modifying code still require an explicit architecture routine; do not
let callers assume a plain C store is sufficient.

On `#BP`, amd64 reports RIP after the one-byte instruction, so lookup uses RIP-1. To continue over
the breakpoint:

1. keep other CPUs quiescent;
2. restore the original byte and set RIP to the breakpoint address;
3. set Trap Flag and mark this CPU as performing an internal step-over;
4. resume only the owner CPU;
5. on the matching `#DB`, clear TF, reinsert `INT3`, then either resume all CPUs or report a user
   single-step stop.

User-requested single-step and internal step-over are separate states. Hardware debug-register
breakpoints are a later epic because they introduce per-CPU register programming, local/global
scope and context-switch preservation.

### Context, protection, reconnect and hostile packets

The amd64 v1 context should initially include RIP, RFLAGS, RSP, integer registers, segment selectors
and stop-relevant exception fields. Add XMM/extended state only behind capability and size/version;
never serialize the private `HalInterruptFrame` as an ABI. The protocol context has compile-time
layout assertions and golden byte vectors independent of assembly stack layout.

Breakpoint patching must handle read-only executable pages without globally weakening page
protection, preserve mapping/cache attributes, and perform required TLB/cache synchronization. A
breakpoint at an unmapped address, noncanonical address or unsafe transient mapping fails without
table mutation. Reconnect rebuilds host state from the target’s breakpoint table. On debugger loss,
the target never leaves a breakpoint’s original instruction temporarily restored: it completes or
rolls back the internal transition before applying the selected fail-stop/fail-open policy.

Session reset rejects old continue/step/breakpoint packets. Duplicate state-changing request IDs
return the cached prior result instead of applying twice. Bounds and address arithmetic are checked
before access; exact payload lengths are required; unexpected asynchronous packets cannot satisfy
a pending request.

### QEMU scenarios

Automate at least:

- one and four CPUs: set/hit/remove/re-hit one breakpoint;
- two CPUs hitting the same address nearly together;
- continue over an enabled breakpoint and explicit single-step;
- breakpoint on a read-only kernel text page and rejection on unmapped/noncanonical addresses;
- duplicate insert/remove/continue packets and stale session replay;
- disconnect while stopped and during the restore/single-step/reinsert sequence;
- reconnect and enumerate target-owned breakpoints;
- malformed/truncated/oversized context and breakpoint packets; and
- normal unowned `#BP`/`#DB` behavior with no debugger.

## Detailed epic plans

### Epic D1: Headless/scriptable debugger

**Objective / done.** One session core serves curses and a non-curses CLI. A script can connect,
wait for a stop/print predicate, issue supported v0/v1 commands, emit stable text or JSONL and exit
with documented codes/timeouts. Existing TUI behavior remains available.

**Source.** `src/debugger/{main,connection,receiver,command,protocol,interface,utils}.py`; add focused
package modules/tests under `src/debugger` or a repository test tree. Update the debugger README
dependency/usage text when implementation occurs.

**Decisions.** Typed event and command API; stable JSON schema version; sync versus selector loop;
stdin script grammar versus one-command CLI. Prefer a small command grammar shared with TUI and a
JSON event schema, not Python plugin execution.

**Phases / reviewable tasks.** (1) Golden current-packet and command-parser tests. (2) Extract pure
codec and event types. (3) Extract session and fake transport. (4) Adapt TUI. (5) Add headless
frontend, deadlines and exit codes. (6) Exercise current target in local QEMU.

**Tests first.** Fake target traces for connect/print/read, wrong peer, empty/truncated datagrams,
timeout and reconnect. Snapshot text/JSONL output; assert headless import path never imports curses.

**Risks.** Accidental v0 behavior change; two state machines during migration; UI thread safety.
Depends on Epic V1 only for the final QEMU gate. Commit boundaries should follow each phase. Do not
combine the extraction with protocol v1 or new commands. **Model:** Sol/high for session extraction,
Sol/medium for frontend; Luna/xhigh for trace/test review. **Subagents:** helpful only after the event
API freezes—one may own fake-target/tests and another the headless frontend; one owner integrates
session state.

### Epic D2: Protocol cleanup and versioning

**Objective / done.** A written v1 ABI, C/Python codecs and golden vectors agree; negotiation,
capabilities, status, correlation, sessions, exact limits and replay behavior are implemented;
malformed input cannot crash, leak mappings or mutate state.

**Source.** `src/kernel/kd/protocol.c`, `initialize.c`, `print.c`, private `kdpdefs.h/kdptypes.h/
kdpfuncs.h`; `src/debugger/protocol.py`, new codec/session modules; ABI documentation.

**Decisions.** Envelope fields/widths, endianness, maximum payload, request idempotency classes,
v0 sunset, context versioning, trusted-network threat boundary. Do not copy undocumented Microsoft
KD packet layouts.

**Phases.** (1) RFC and threat model. (2) Cross-language vectors and fuzzable decoders. (3) Target
read-only requests with structured errors. (4) session/replay rules. (5) async stop/print events.
(6) temporary dual negotiation and telemetry. (7) delete v0 at the agreed gate.

**Tests first.** Every boundary length; integer overflow; extra trailing data; unknown capability/
type/status; wrong peer/session; reorder, duplicate and retry; mapping exception cleanup; print
truncation; parser fuzzing. **Risks:** boot deadlock, UDP amplification, divergent packed layouts,
legacy ambiguity. Depends on D1’s codec/session tests and V1. Commit RFC/vectors before target
changes; land decoder before encoder switch. Do not combine with breakpoints or transport changes.
**Model:** Sol/xhigh for ABI/threat design, Sol/high implementation. **Subagents:** useful for an
independent codec/fuzz review after one ABI owner freezes the RFC; not for parallel ABI design.

### Epic D3: amd64 breakpoints, stop/resume and stepping

**Objective / done.** Stop reasons and amd64 contexts are reliable; continue/explicit stop/single
step work; target-owned `INT3` breakpoints survive step-over and reconnect; SMP and debugger-loss
policies pass the scenarios above.

**Source.** `src/kernel/kd/{initialize,protocol}.c` plus a stop/breakpoint module; `src/kernel/hal/
amd64/{idt.c,idt.S,smp.c,map.c}`; paired context headers/assembly constants; `src/kernel/ke/ipi.c`;
debugger commands/session. Export lists change only if a genuine public interface is required.

**Decisions.** exception ownership, stop IRQL/interrupt state, reversible CPU rendezvous, patch and
cache hook, context ABI, table bound, debugger-loss policy, behavior when a CPU cannot quiesce.

**Phases.** (1) context/stop event with read-only registers. (2) reversible rendezvous and manual
stop/continue. (3) architecture patch primitive and bounded table. (4) `#BP` ownership. (5)
internal TF step-over. (6) user step. (7) reconnect/loss and SMP stress. Hardware breakpoints later.

**Tests first.** Unit model of breakpoint state transitions plus QEMU matrix above. Instrument
original-byte/table consistency on every transition. **Risks:** deadlock on stopped locks, lost CPU,
text corruption, swallowing real exceptions, stale TF, breakpoint races. Depends on D2 and C1/C2
invariant work. Commit stop controller separately from exception hook, patching separately from
step-over, and commands last. Do not combine with ARM64 exception work or HAL contract refactoring.
**Model:** Sol/xhigh design and implementation; use max only to adjudicate an actual Intel/manual
or unwind-frame contradiction. **Subagents:** valuable for independent exception/SMP review and
QEMU scenario implementation after ownership is fixed; breakpoint state has one code owner.

### Epic D4: Command expansion

**Objective / done.** TUI and headless expose the same typed operations, help and errors. Initial
set: capabilities, target status, CPUs, register/context read, memory read, breakpoint list/add/
remove/enable, stop, continue and step. Then consider memory write, module list/symbol lookup and
backtrace once safe primitives exist.

**Source.** debugger command/service/frontends; target protocol handlers; kernel module list and
RT unwind interfaces only through explicit KD-safe adapters.

**Decisions.** command grammar and JSON result schemas; address-space/CPU selection; which commands
are legal while running; pagination/size bounds; symbol provenance. Port I/O remains amd64-
capability-gated.

**Phases.** Add one target operation, codec vector, service method and both frontend renderings per
commit. Context and execution control precede convenience commands. **Tests first:** parser edge
cases, capability rejection, stopped/running legality, large output bounds and headless snapshots.
**Risks:** command surface outruns protocol safety; lock-taking/backtrace while stopped. Depends on
D1–D3. Do not bundle a large command batch or symbol-file parser with breakpoint core. **Model:**
Sol/medium per bounded command; Sol/high for backtrace/write-memory; Luna/xhigh review/tests.
**Subagents:** useful for independent commands with disjoint packet types after service API freeze.

### Epic K1: Extension-module discovery and loading

**Objective / done.** The loader discovers valid candidate module identities from device/firmware
classification, validates them as hostile PE input, tries candidates deterministically, records
why each failed, and passes an architecture/transport-neutral descriptor to the kernel. Proprietary
artifacts remain externally supplied and untracked.

**Source.** `src/boot/osloader/main.c`, PCI/ACPI/PE/config/filesystem helpers, `include/platform.h`;
`run.sh` image-copy configuration; kernel KD device/initializer adapter.

**Primary contract.** Microsoft documents PCI module names `kd_YY_XXXX.dll` where `YY` is PCI class
and `XXXX` is vendor ID, and DBG2-derived non-PCI names `kd_XXXX_YYYY.dll` using port type/subtype.
It also documents a single explicit `KdInitializeLibrary` export and no imports, and distinguishes
packet and byte transports in the supplied function table
([KDNET extensibility modules](https://learn.microsoft.com/en-us/windows-hardware/drivers/debugger/how-to-develop-kdnet-extensibility-modules)).
[SPEC] The current `kd_02_%04X.dll` Ethernet/vendor construction is therefore narrower than the
public contract. [CODE]

**Decisions.** candidate ordering, PCI class/subclass encoding, DBG2 parsing boundary, module
allow-list configuration, fallback/error diagnostics, boot-block descriptor version. Never infer
module support only from filename.

**Phases.** (1) pure candidate-name tests. (2) generalized PCI identity with existing Ethernet
behavior. (3) strict PE/export/import/machine validation and reason codes. (4) externally configured
candidate list. (5) DBG2 inventory/parser after ACPI table validation is reusable. (6) local
compatibility matrix recording Windows build, hash, PE metadata, initialized export capabilities
and runtime result—never the binary.

**Tests first.** synthetic legal/illegal PE modules, wrong machine, multiple/missing exports,
imports, overflow/truncation, candidate fallback and duplicate names. **Risks:** undocumented module
dependencies, loader-block ABI changes, confusing “loads” with “works”. Depends on V1 and PE loader
hardening, but not on K2. Commit naming, validation, configuration and DBG2 separately. Do not mix
with network/USB implementation. **Model:** Sol/high loader/ABI; Luna/xhigh compatibility inventory.
**Subagents:** helpful for synthetic PE corpus and independent public-document review; proprietary
observations need a single controlled evidence log.

### Epic K2: KD transport abstraction

**Objective / done.** Palladium protocol/session code consumes a narrow `KdpTransportOps`; existing
KDNET Ethernet remains behaviorally equivalent; transport buffers are paired on every path; packet
and byte-oriented adapters can be represented without protocol duplication.

**Source.** all `src/kernel/kd` network, initialize, import/export and protocol files; private KD
types/functions; loader debug descriptor. No public kernel export is needed unless a future driver
supplies a transport.

**Decisions.** lifecycle (`initialize`, `poll`, `shutdown`), packet receive/release and transmit
acquire/send pairs, MTU/capabilities, byte-stream framing adapter, polling/interrupt constraints,
IRQL/allocation rules, loss/reset indication. `UseSerialFraming` and USB-related fields in the
extension ABI are evidence of possible modes, not proof of semantics. [CODE]/[QUESTION]

M0's amd64 PC16550 diagnostic output is a bounded one-way HAL sink, not a KD transport. When K2/K3
begin, evaluate a Palladium-owned bidirectional PC16550 KD adapter alongside native USB and
extension-backed transports; it would avoid a proprietary module for serial debugging. Decide then
whether the one-way diagnostic mechanism remains a HAL facility, shares only an amd64 UART device
primitive with KD, or is superseded. Do not make KD protocol/session code call the M0 diagnostic
write API, and do not move transport ownership into the HAL merely because UART PIO is an amd64 HAL
mechanism. [ARCHITECTURAL RECOMMENDATION]

**Phases.** (1) document current ownership pairs. (2) wrap extension buffers with no behavior
change. (3) move Ethernet/IP/UDP behind a packet adapter. (4) isolate protocol from network tuple.
(5) add an in-memory/fault-injection transport. (6) only then add a second real transport.

**Tests first.** acquire-without-send, receive-without-release, zero/oversized frames, reset during
request, duplicated packet and constrained MTU; current QEMU KDNET trace equivalence. **Risks:**
leaks/deadlocks inside proprietary callbacks, abstraction that assumes packet semantics, boot-time
allocation. Depends on D2 and K1’s descriptor; should follow stable protocol lifecycle. One commit
per ownership seam. Do not combine with KDUSB. **Model:** Sol/high; Luna/xhigh pairing/fault review.
**Subagents:** useful for network adapter tests after the ops contract is owned centrally.

### Epic K3: KDUSB feasibility

**Objective / done.** Produce a go/no-go design note backed by hardware/emulator inventory and
legally obtained module observations. A “go” identifies the exact controller capability, firmware
description, cable/host setup, module/export mode, memory/DMA/cache requirements and automated
test path. It does not yet promise a transport implementation.

**Sources to inspect.** UEFI/ACPI DBG2 data; xHCI PCI extended capability and Debug Capability;
loader KD candidate logic; K2 ops; available QEMU xHCI model; legally obtained Windows modules.
Microsoft’s public setup requires an xHCI controller exposing a USB 3 debug port and a suitable
debug cable ([USB 3.0 debugging setup](https://learn.microsoft.com/en-us/windows-hardware/drivers/debugger/setting-up-a-usb-3-0-debug-cable-connection)).
[SPEC] Kernel-debug transport choices include network, USB and serial
([KD transport overview](https://learn.microsoft.com/en-us/windows-hardware/drivers/debugger/performing-kernel-mode-debugging-using-kd)).
[SPEC]

**Research phases.** (1) spec map for xHCI DbC and DBG2. (2) enumerate QEMU support and physical
hardware. (3) record module identities/PE/callback capabilities without redistribution. (4) build a
minimal extension initialization probe behind K2. (5) decide: external module adapter, native
Palladium DbC transport, or defer.

**Acceptance evidence.** Controller found; debug capability and port identified; host enumerates;
bounded send/receive survives reset; failure diagnostics are captured. If QEMU cannot model DbC,
hardware-only status is explicit. **Risks:** controller/firmware variability, DMA below/above 4 GiB,
cache coherency, ownership conflict with future USB driver, proprietary module dependencies.
Depends on K1/K2 and available hardware. Keep research, loader support and implementation as
separate commits/epics. **Model:** Sol/xhigh for spec/module synthesis; Luna/xhigh for inventory.
**Subagents:** helpful for independent QEMU and hardware/spec investigations if each reports source
and evidence class; not for reverse engineering beyond the legal scope.
