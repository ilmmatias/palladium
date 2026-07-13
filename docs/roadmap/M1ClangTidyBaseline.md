# M1 clang-tidy baseline

This is the immutable starting classification for M1. It records the full 690-diagnostic result
produced on 2026-07-12 by `run-clang-tidy` from LLVM 22.1.8 over the amd64 RelWithDebInfo compile
database. It is evidence about that toolchain and revision, not a promise that another LLVM release
will produce identical diagnostics.

| Check | Count | M1 disposition |
|---|---:|---|
| `bugprone-macro-parentheses` | 478 | Classified noise/intentional low-level macros; non-gating, review only when a macro changes |
| `bugprone-narrowing-conversions` | 86 | Classified; audit ABI and arithmetic boundaries, not globally gated |
| `bugprone-switch-missing-default-case` | 21 | Classified; add defaults only where malformed external input can reach the switch |
| `bugprone-signed-char-misuse,cert-str34-c` | 19 | Actionable CRT/input queue; fix callers that pass plain `char` to ctype |
| `misc-no-recursion` | 9 | Intentional/custom AML and namespace recursion; requires depth limits, not suppression as a fix |
| `bugprone-multi-level-implicit-pointer-conversion` | 9 | ABI boundary review queue |
| `bugprone-branch-clone` | 9 | Maintainability queue; non-gating |
| `misc-redundant-expression` | 7 | Mostly imported mathematical identities; verify locally before changing |
| `clang-analyzer-core.FixedAddressDereference` | 7 | Intentional amd64 recursive/fixed mappings; locally justify, non-gating |
| `cert-err33-c` | 7 | Error-path audit queue |
| `clang-analyzer-security.insecureAPI.strcpy` | 6 | Bounds audit queue; each destination must be proven or bounded |
| `bugprone-misplaced-widening-cast` | 5 | Arithmetic audit queue |
| `bugprone-suspicious-include` | 4 | Intentional SHA implementation sharing; non-gating |
| `clang-analyzer-core.UndefinedBinaryOperatorResult` | 3 | High-signal gate; demonstrated scanner/unwind defects |
| `clang-analyzer-core.NullDereference` | 3 | High-signal gate; loader/kernel/RT control flow |
| `bugprone-inc-dec-in-conditions` | 3 | Readability and sequencing queue |
| `bugprone-casting-through-void` | 3 | Unwind layout review queue |
| `clang-analyzer-security.ArrayBound` | 2 | High-signal gate; pool bucket and PCI BAR bounds |
| `clang-analyzer-deadcode.DeadStores` | 2 | Review queue |
| `bugprone-unchecked-string-to-number-conversion,cert-err34-c` | 2 | Boot configuration malformed-input queue |
| `clang-analyzer-core.uninitialized.UndefReturn` | 1 | High-signal gate; ACPI field result |
| `clang-analyzer-core.DivideZero` | 1 | High-signal gate; video geometry |
| `clang-analyzer-core.CallAndMessage` | 1 | High-signal gate; KD formatting state |
| `clang-analyzer-core.BitwiseShift` | 1 | High-signal gate; ACPI 64-bit field mask |
| `bugprone-suspicious-string-compare` | 1 | Explicit comparison cleanup |

The 212 diagnostics with directly attributable project source locations were distributed as:

| Source area | Count |
|---|---:|
| `src/sdk/crt` | 84 |
| `src/drivers/acpi` | 64 |
| `src/kernel` | 26 |
| `src/boot/osloader` | 26 |
| `src/sdk/rt` | 7 |
| `src/sdk/crypt` | 5 |

The remaining 478 are the macro-parentheses family emitted through macro expansion and are tracked
as one non-gating family rather than assigned misleading subsystem ownership.

M1 gates only the analyzer null/undefined/divide/shift/array-bound set through
`tools/run-clang-tidy-high-signal.sh`. Fixed-address diagnostics and broad conversion/macro families
remain outside that gate. Any confirmed defect gets a focused regression; intentional low-level
constructs get a narrow local explanation rather than a global suppression.
