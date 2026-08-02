# Core decompilation subsystems

The initial decompilation slice follows the data flow most useful for matching
Melee code:

```text
frontend IR optimizer
        |
        v
CodeGen_Generator -> PCode optimizer -> scheduler/peephole
        |                                  |
        v                                  v
interference graph -> coloring -> spill insertion and retry
        |
        v
EABI stack layout -> prologue/epilogue -> final scheduling/emission
```

`config/GC_1_2_5/subsystems.json` is the machine-checkable index. It records
each working function name, exact address, source-file anchor, role, and
evidence level. `ninja check` verifies every function lies in `.text`, every
source placeholder exists, every function marked `decompiled` has a body in
its assigned source, and every trace string still matches the verified stock
executable byte-for-byte.

Print the current reconstructed-function and match table with:

```sh
python3 tools/decomp_status.py
```

CodeWarrior Pro 5 Win32/x86 2.3 now provides a validated matching baseline.
Six small register-state functions are instruction-exact at
`-O4,p -inline auto`. A remaining “unmeasured” entry means its reconstructed
translation unit has not yet been compiled and mapped, not that no usable host
compiler exists. Functional status can still advance independently when exact
target control flow and observable state changes have been reconstructed.

## First vertical slices

### Backend coordinator

Recover `CodeGen_Generator` at `0x004351c0` first. It provides exact sequencing
and the live objects passed between otherwise opaque modules. Keep diagnostic
dump calls in the reconstruction until all stage boundaries are understood.

### Optimizer

Recover the small dispatcher at `0x004c4430`, then level 3 at `0x004c4910` and
level 4 at `0x004c4530`. The repeated calls are the result we care about: do not
refactor them into a unique-pass list. Trace strings at `0x0056200c` through
`0x00562188` make pass identification auditable.

The first `CodeMotion.c` control slice is reconstructed and measured.
`COpt_00521a10` guards a recursive walk of the active code-motion tree.
`COpt_SetLoopCodeMotionMode` at `0x00523650` optionally rebuilds an
object-keyed collection from eligible PCode instructions, initializes eight
bitsets per basic block, and invokes four analysis stages in a fixed order.
`COpt_00524bd0` resets the change state, runs two guarded tree passes, and
releases iteration storage. The address-only names remain conservative until
the node passes themselves establish stronger roles.

This slice rejects the earlier Boolean interpretation of `0x0058763c`: it is
the active `CodeMotionNode*` tree root. The optimizer dispatcher tests the
pointer for null and passes the same value into the neighboring tree routines.

The validated Pro 5 candidate measures these functions at 0.00%, 80.21%, and
20.69% positional comparable bytes respectively. Both low-scoring wrappers
have the target operation sequence after removing a candidate-only EBP frame;
the setup routine has the same complete eight-allocation loop and final pass
order. Its remaining body differences are an iterator-register permutation
and one global reset that the candidate folds to an immediate.

The three recursive walkers are also reconstructed. `COpt_00521a30` and
`COpt_00524c10` visit siblings and process each node after recursively
processing its children. `COpt_00525070` applies its action only to leaves
whose byte at `+0x4f` is clear. Plain recursive C is the retained source shape.
Retail expands eight processing levels before leaving a residual recursive
call, while the validated candidate expands seven. `-inline auto,level=8` is
byte-neutral, an explicit `inline` qualifier overshoots to eleven levels, and
factoring the node body into a helper falls to four. The gap is therefore a
specific recursive-inliner fingerprint, not a reason to hand-unroll the C.

### Register allocation

Start with the coloring coordinator at `0x004cdef0`. It processes vector,
general-purpose, and floating-point classes independently and retries classes
after spill insertion. Work outward into `Coloring.c`, the `Registers.c` state
helpers, and finally the six directly anchored `SpillCode.c` functions.

For each class, recover these boundaries independently:

1. virtual-register census and allocatable physical set;
2. interference construction;
3. coalescing;
4. simplification and spill candidate selection;
5. coloring/physical assignment;
6. spill-code insertion and retry;
7. callee-saved register reporting to frame construction.

The shared explicit-binding and automatic-allocation layer is reconstructed in
`Registers.c`. It accounts for physical use and saved-register ranges for all
three classes, including paired GPR values. The next allocator target is the
interference-node lifecycle: census, edge construction, coalescing, simplify,
color selection, and spill retry state. The physical-state snapshot/restore,
initial color masks, availability counts, and saved-color claim helpers are
also reconstructed for all three classes; GPR coloring begins with r0-r12 and
claims additional colors downward from r31 through r14.

### PCode descriptions

The exact opcode descriptor table at `0x005654b0` is exported to
`build/GC_1_2_5/pcode-opcodes.json` by `ninja pcode-opcodes`. It contains all
466 opcodes through `0x1d1`, including mnemonic, operand-format string, fixed
operand count, flags, and base PowerPC encoding. Allocator snapshots read the
same table from live compiler memory and attach the descriptor to every PCode
instruction.

The compact format strings are executable metadata, not display-only names.
`PCodeUtilities` at `0x004a2660` uses them to allocate the variable-length
instruction and construct each 12-byte operand, including use (`flag 1`),
definition (`flag 2`), and read-modify-write (`flag 3`) roles. Recovering that
format interpreter is represented by `decode_operand_format`, so both the
static catalog and live snapshots expose normalized use/definition roles,
possible operand kinds, and fixed or dynamic expansions.

The shared interpreter is now reconstructed as typed C, including object
provenance in the `m`, `M`, and `l` cases. Object tag 5 is required for
object-backed operands. Object kind 2 becomes an immediate-form operand;
ordinary objects retain their identity in a memory-form operand, while null
objects become raw immediates. Type/object flag bits feed instruction flags
`0x10000` and `0x20000` for eligible GPR-result opcodes. The address-backed
classifier at `0x0048ad10`, which chooses one access-flag subcase for ordinary
`m` objects, remains the next leaf to recover.

### Stack frame

The generator calls `0x004abe90` immediately before the “AFTER GENERATING
EPILOGUE, PROLOGUE” boundary. `0x004aba30` participates before “AFTER MERGING
EPILOGUE, PROLOGUE.” Recover frame regions separately: linkage/save area,
outgoing arguments, spills, compiler temporaries, locals, incoming arguments,
alignment, and dynamic/large-frame special cases.

## Evidence discipline

- **confirmed**: established directly by this exact executable;
- **inferred**: a strong semantic conclusion from its control flow or xrefs;
- **reference**: borrowed as a hypothesis from another version.

Promote a name or layout only with an address-backed observation. A neighboring
MWCC version can suggest what to test, but never overrides this target.
