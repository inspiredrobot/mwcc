/*
 * CodeGen.c
 *
 * Core entry point:
 *   0x004351c0  CodeGen_Generator
 *
 * This function coordinates PCode construction, backend optimization,
 * scheduling, register coloring, EABI frame construction, final peephole
 * optimization, and emission. Recover the coordinator before its large
 * helpers so every pass boundary remains visible and testable.
 */

#include "mwcc/backend_types.h"
#include "mwcc/Registers.h"

typedef struct CodeGen_DecompPending CodeGen_DecompPending;

/*
 * Object-preallocation / virtual-register numbering pass.
 *
 *   0x00437230  CodeGen_PreallocateObjectRegisters
 *
 * This is the routine that assigns virtual-register NUMBERS to every
 * register-candidate compiler object before PCode lowering emits any
 * instruction.  `Registers_AllocateGPR`/`AllocateFPR`/`AllocateGPRPair`/
 * `AllocateVR` each consume the next value of a single monotonic per-class
 * counter (GPR is `gUsedVirtualRegistersGPR` at 0x0058846e), so a web's
 * virtual-register number is exactly its position in the walk order below.
 * That number is what `Coloring_SimplifyGraph` later uses as the pop/claim
 * tie-break, so the walk order below is the ultimate source of the "web N
 * colored rK" decision for every object-backed value.
 *
 * The pass walks several distinct object lists IN ORDER.  Each list holds a
 * different object stratum; concatenating them produces the numbering:
 *
 *   1. list @0x005882ac                  -> the "initial object" stratum
 *   0x004c1950  Registers_SnapshotInitialObjectRange
 *                                        records InitialObjectLast = counter-1
 *   2. list @0x0058806c                  -> post-initial objects
 *   3. list @0x00587fb8  (first pass)    -> local-object list, pass 1
 *   0x004c1980  Registers_BeginCoalesceWindow
 *                                        sets CoalesceFirst = CoalesceLast =
 *                                        the current counter (the boundary
 *                                        below which webs may not coalesce)
 *   4. list @0x00587fb8  (second pass)   -> local-object list, pass 2
 *   5. list @0x005876a0                  -> trailing objects
 *
 * The coalesce boundary therefore falls BETWEEN the two passes over the
 * local-object list.  Objects numbered before it are the non-coalescable
 * "initial + shadow" stratum; the first object numbered by pass 2 receives
 * `gGPRCoalesceFirst` exactly.  In the GALE01 `efAsync_Dispatch` capture that
 * boundary is 70, and the six common-subexpression recompute loads that
 * `Coloring_SelectColors` later colors r24 occupy vregs 70..75 -- i.e. they
 * are the very first objects of pass 2, which is why they out-rank (pop after)
 * the canonical main-lowering loads numbered 76+.  This resolves the
 * "shadow-object grant pass" gap noted in docs/DATA_MODEL.md: the shadow /
 * recompute stratum is granted here, by the second and later list walks,
 * ahead of the bulk of lowering.
 *
 * High-level equivalent; binary match unmeasured.
 */

extern ObjectList* gInitialObjectList_005882ac;
extern ObjectList* gPostInitialObjectList_0058806c;
extern ObjectList* gLocalObjectList_00587fb8;
extern ObjectList* gTrailingObjectList_005876a0;
extern unsigned char gColoringGuard_00584244;

/*
 * Per-object dispatch shared by every walk (0x00437241 / 0x00437308 body).
 * `wants_vector` selects the trailing vector case that only the post-initial
 * and local-object walks include (walks 2-4); the first (0x005882ac) and last
 * (0x005876a0) walks omit it.
 */
static void codegen_preallocate_object(CompilerObject* object,
                                       int wants_vector)
{
    RegisterInfo* info;
    CompilerType* type;
    unsigned int pair_flags;
    unsigned char kind;

    info = Registers_GetInfo(object); /* 0x004c1720 */
    if (info->flags_23 == 0) {        /* not a register candidate */
        return;
    }
    if (info->flags_22 != 0) { /* explicitly excluded */
        return;
    }

    type = object->type;
    if (type->kind == 0x0b) { /* wrapped view carries the pair flags */
        pair_flags = type->flags_0a;
    } else {
        pair_flags = object->flags_12;
    }
    if (pair_flags & 2) { /* second half of a value pair */
        return;
    }
    if (info->physical_register !=
        0) { /* already numbered by an earlier walk */
        return;
    }

    kind = type->kind;
    if (kind == 1 || kind == 3 || kind == 0x0b ||
        (kind == 0x0a && type->size == 4) ||
        (gColoringGuard_00584244 && kind == 2))
    {
        if ((kind == 1 || kind == 3) && type->size == 8) {
            Registers_AllocateGPRPair(object); /* 0x004c2120 */
        } else {
            Registers_AllocateGPR(object); /* 0x004c2280 */
        }
    } else if (kind == 2) {
        Registers_AllocateFPR(object); /* 0x004c2040 */
    }

    if (wants_vector && kind == 4 && type->subtype >= 4 && type->subtype <= 14)
    {
        Registers_AllocateVR(object); /* 0x004c1f60 */
    }
}

static void codegen_preallocate_list(ObjectList* list, int wants_vector)
{
    ObjectList* node;

    for (node = list; node != NULL; node = node->next) {
        codegen_preallocate_object(node->object, wants_vector);
    }
}

/* 0x00437230; high-level equivalent; binary match unmeasured. */
void CodeGen_PreallocateObjectRegisters(void)
{
    codegen_preallocate_list(gInitialObjectList_005882ac, 0);
    Registers_SnapshotInitialObjectRange(); /* 0x004c1950 */
    codegen_preallocate_list(gPostInitialObjectList_0058806c, 1);
    codegen_preallocate_list(gLocalObjectList_00587fb8, 1);
    Registers_BeginCoalesceWindow(); /* 0x004c1980 */
    codegen_preallocate_list(gLocalObjectList_00587fb8, 1);
    codegen_preallocate_list(gTrailingObjectList_005876a0, 0);
}
