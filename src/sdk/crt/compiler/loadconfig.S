/* SPDX-FileCopyrightText: (C) 2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

.altmacro
.section .rdata, "dr"

/* No access to __UINT*_WIDTH__ here; We really should explore the compiler a bit more to see if
 * there is any other useful macro that we can use. */
#if ARCH_amd64
#define PTR quad
#else
#define PTR long
#endif /* ARCH_* */

/* I think that for COFF/PE, x86's is the only one that uses the underscore prefix for symbols? */
#ifdef ARCH_x86
#define SYM(Name) _##Name
#else
#define SYM(Name) Name
#endif /* ARCH_x86 */

/* This would be __security_cookie under MSVC (or clang-cl), but we can't use clang-cl (because it
 * depends on the Windows SDK, but we're not Windows!), so we're forced to usethe GCC-style stack
 * chk guard pointer instead. */
.extern SYM(__stack_chk_guard)

.global SYM(_load_config_used)
.align 16
SYM(_load_config_used):
    /* Size */ .long SYM(_load_config_used_end) - SYM(_load_config_used)
    /* TimeDateStamp */ .long 0
    /* MajorVersion */ .short 0
    /* MinorVersion */ .short 0
    /* GlobalFlagsClear */ .long 0
    /* GlobalFlagsSet */ .long 0
    /* CriticalSectionDefaultTimeout */ .long 0
    /* DeCommitFreeBlockThreshold */ .PTR 0
    /* DeCommitTotalFreeThreshold */ .PTR 0
    /* LockPrefixTable */ .PTR 0
    /* MaximumAllocationSize */ .PTR 0
    /* VirtualMemoryThreshold */ .PTR 0
    /* ProcessAffinityMask */ .PTR 0
    /* ProcessHeapFlags */ .long 0
    /* CsdVersion */ .short 0
    /* DependentLoadFlags */ .short 0
    /* EditList */ .PTR 0
    /* SecurityCookie */ .PTR SYM(__stack_chk_guard)
    /* SeHandlerTable */ .PTR 0
    /* SeHandlerCount */ .PTR 0
    /* GuardCfCheckFunctionPointer */ .PTR 0
    /* GuardCfDispatchFunctionPointer */ .PTR 0
    /* GuardCfFunctionTable */ .PTR 0
    /* GuardCfFunctionCount */ .PTR 0
    /* GuardFlags */ .long 0
    /* CodeIntegrity.Flags */ .short 0
    /* CodeIntegrity.Catalog */ .short 0
    /* CodeIntegrity.CatalogOffset */ .long 0
    /* CodeIntegrity.Reserved */ .long 0
    /* GuardAddressTakenIatEntryTable */ .PTR 0
    /* GuardAddressTakenIatEntryCount */ .PTR 0
    /* GuardLongJumpTargetTable */ .PTR 0
    /* GuardLongJumpTargetCount */ .PTR 0
    /* DynamicValueRelocTable */ .PTR 0
    /* ChPeMetadataPointer */ .PTR 0
    /* GuardRfFailureRoutine */ .PTR 0
    /* GuardRfFailureRoutineFunctionPointer */ .PTR 0
    /* DynamicValueRelocTableOffset */ .long 0
    /* DynamicValueRelocTableSection */ .short 0
    /* Reserved2 */ .short 0
    /* GuardRfVerifyStackPointerFunctionPointer */ .PTR 0
    /* HotPatchTableOffset */ .long 0
    /* Reserved3 */ .long 0
    /* EnclaveConfigurationPointer */ .PTR 0
    /* VolatileMetadataPointer */ .PTR 0
    /* GuardEhContinuationTable */ .PTR 0
    /* GuardEhContinuationCount */ .PTR 0
    /* GuardXfgCheckFunctionPointer */ .PTR 0
    /* GuardXfgDispatchFunctionPointer */ .PTR 0
    /* GuardXfgTableDispatchFunctionPointer */ .PTR 0
    /* CastGuardOsDeterminedFailureMode */ .PTR 0
    /* GuardMemcpyFunctionPointer */ .PTR 0
    /* UmaFunctionPointers */ .PTR 0
SYM(_load_config_used_end):
