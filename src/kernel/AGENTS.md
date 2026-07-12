# Kernel guidance

This scope covers the executive (`ke`), events (`ev`), HAL, debugger (`kd`), memory manager (`mm`),
objects (`ob`), scheduler/threads (`ps`), display (`vid`), and kernel headers/exports. Build the
`kernel` target through the repository build and smoke-boot it for runtime-sensitive changes.

- Public interfaces are assembled by `include/public/kernel/*.h` from `detail/*defs.h`,
  `*types.h`, `*funcs.h`, and `*inline.h`; private equivalents use the `p` namespace. Update the
  correct layer and `kernel.def` when an exported ABI changes. Preserve `IWYU pragma: private` and
  architecture include routing.
- The only implemented architecture is amd64. Keep generic contracts outside `hal/amd64` and
  `detail/amd64`; keep architecture layouts, `.inc` constants, assembly unwind directives, and C
  structures synchronized. In particular, the IRQL constants in `detail/amd64/kedefs.h` and
  `private/kernel/detail/amd64/irql.inc` must agree.
- Treat IRQL as part of every routine's contract. `*AtCurrentIrql` lock helpers do not raise or
  validate IRQL. Restore the exact previous IRQL on every exit. Pool allocation/free is used at no
  higher than DISPATCH in current code; scheduler/context-switch paths deliberately raise to SYNCH.
  Inspect local comments and all callers before moving allocations, waits, or callbacks across
  levels.
- Preserve lock-protected state transitions and atomic memory ordering. Do not call blocking waits
  while holding spin locks. When touching event, processor, wait-tree, run-queue, alert, or
  termination state, audit competing timeout, wakeup, interrupt, and other-CPU paths together.
- `PsThread.EventHeader` intentionally inlines the `EvHeader` layout because of header dependencies;
  keep them synchronized. Context-frame `Busy`, queue counts, thread states, and reference transfers
  are scheduler invariants, not bookkeeping conveniences.
- `ObCreateObject` returns a body with one reference; directory insertion holds an additional
  reference, and final dereference invokes the type delete routine and frees with the stored tag.
  Audit whether lookup/callback results are borrowed or owned from their callers before changing
  lifetime behavior. Do not mix pool tags between allocation and free.
- Distinguish early MM/HAL allocation and mapping from the online pool/page allocators. Preserve
  recursive page-table assumptions, TLB invalidation/shootdown, page/PTE accounting, per-processor
  caches, alignment, and rollback after partial allocation. Reconcile virtual layout changes with
  `docs/amd64/AddressSpace.txt`.
- Fatal errors are appropriate for broken internal state (wrong IRQL/thread state, corrupt pool/PFN,
  failed required initialization). Allocation failure and externally malformed data should follow
  existing recoverable paths until initialization cannot safely continue.
- Panic/KD/display ownership paths deliberately bypass normal locking after processors are frozen;
  do not generalize those escape hatches to ordinary execution.

Verification: build the kernel with warnings as errors. For MM, HAL, scheduler, synchronization,
object, interrupt, or ABI changes, boot with multiple QEMU CPUs (the script uses all host CPUs),
check the kernel banner, managed-memory count, processor-online count, driver startup, and absence
of fatal-error diagnostics. No automated race, stress, or hardware suite exists; state what was not
exercised.
