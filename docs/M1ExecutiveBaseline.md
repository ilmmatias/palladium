# M1 trustworthy amd64 executive baseline

M1 adds executable evidence around the current executive without changing Palladium into a
different kernel design. A normal Release build and M0 smoke boot remain the primary shipping
lane. `PALLADIUM_ENABLE_SELF_TESTS=ON` creates a separate RelWithDebInfo test kernel and is the only
configuration that compiles the private target tests and page-accounting rendezvous.

## Frozen contracts

PFN accounting uses one conservation equation:

```text
Used + Cached + Free + Reserved == Managed
```

Managed covers loader descriptor types 0 through 6. Type 7 system-reserved/MMIO ranges are counted
as unmanaged and never size the pool or PFN allocator. OS-loader temporary pages remain Used until
boot-region release; firmware-temporary pages are Free after `ExitBootServices`; firmware-permanent
pages remain Reserved. Boot, graphics, page-table, PFN, and pool counts are diagnostic subsets of
Used rather than additional terms.

Signals remain manual-reset and wake all waiters. A zero timeout is a poll and never queues or
switches. Blocking waits require IRQL below SYNCH. Signal and timeout completion compete through a
single generation-bound winner. Mutex recursion is checked, contention describes pending waiters,
and release chooses handoff from the actual wait list. A thread owning a mutex at termination is a
fatal invariant until abandoned-mutex semantics are designed.

`PsResumeThread` performs one transition only: SUSPENDED to QUEUED. It returns `true` only to the
caller which wins that transition. Queued `KeWork` remains caller-owned through callback completion.
Queued alerts hold a thread reference. Caller-owned alerts clear their queued bit after execution or
discard; pool-owned alerts remain non-requeueable until they are freed.

`ObLookupDirectoryEntryByName` returns a referenced object. Indexed lookup reports SUCCESS,
NOT_FOUND, or BUFFER_TOO_SMALL and returns a reference only on success. Removal requires both the
referenced directory and object. Directory mutation follows tree-lock then directory-lock order;
destructors run after those locks are released.

amd64 unwind accepts versions 1 and 2 only. Version 2 uses `UWOP_EPILOG` descriptors to decide
whether the control PC is in an epilog, then validates and simulates the instruction sequence.
Versions 0, 3, unknown opcodes, and truncated multi-slot operations are rejected without modifying
the context. Version 3 remains a later, independent implementation.

## Evidence lanes

The host CTest project runs portable list, bitmap, AVL, hash, scanner, and random-number vectors
under ASan/UBSan. It compiles the production source files directly with `-ffreestanding` and
`-fno-builtin`; renamed standard symbols prevent the host libc from silently replacing the
implementation under test.

The target kernel emits stable `M1TEST START`, `PASS`, `FAIL`, and `COMPLETE` records. Current suites
cover portable RT vectors, a quiescent PFN/accounting verifier, immediate and finite waits, mutex
timeout recovery, one-shot resume, referenced object/directory behavior, and amd64 unwind version-2
and malformed vectors. `tools/run-qemu.py selftest` preserves the serial log, fresh firmware state,
input hashes, parsed suite records, duration, and exit reason for every run.

The machine-readable clang-tidy classification is
[`roadmap/M1ClangTidyBaseline.json`](roadmap/M1ClangTidyBaseline.json). The broad 690-diagnostic
baseline remains classified rather than globally gated. The dedicated high-signal analyzer pass is
zero-tolerance for null, undefined-value, divide, shift, and array-bound findings.

## Qualification policy

Each 1-, 2-, and 4-CPU TCG profile becomes required only after 20 consecutive local passes and one
successful hosted contained-branch run. Until that condition is met, its CI step remains explicitly
non-gating. Passing QEMU TCG does not establish hardware behavior, arbitrary firmware coverage, or
the absence of SMP races outside the fixed corpus.

On 2026-07-12, the final local profile completed 20/20 one-CPU boots and one fresh boot each at two
and four CPUs, with 39 cases in every run. The one-CPU CI step remains staged with
`continue-on-error` until the contained branch completes a hosted run. The two- and four-CPU lanes
have not completed their own 20-run qualification and therefore are not CI gates.
