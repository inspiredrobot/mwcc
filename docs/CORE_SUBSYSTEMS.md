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

Binary percentages remain “unmeasured” until the exact Win32 host toolchain is
available. Functional status can advance independently when exact target
control flow and observable state changes have been reconstructed.

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
