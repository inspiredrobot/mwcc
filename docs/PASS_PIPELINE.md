# Optimization pipeline

Status legend: **confirmed** means observed in the stock GC/1.2.5 executable;
**reference** means suggested by another version/tool and awaiting exact-binary
verification.

## Frontend

The executable contains `IrOptimizer.c` and trace strings naming these stages:

- `IRO_LoopUnroller`
- `IRO_FindLoops`
- `IRO_CopyAndConstantPropagation`
- `IRO_ConstantFolding`
- `IRO_EvaluateConditionals`
- `IRO_RangePropagateInFNode`
- `IRO_ExpressionPropagation`

These names are **confirmed**. The dispatcher is at `0x0042cd10`; its bytes are
unchanged in GC/1.2.5n. RootCubed's independently versioned GC/1.2.5 debugger
breakpoint at `0x0042cd1f` corroborates the identification.

## Backend

`CodeGen_Generator` is at `0x004351c0`. Its backend optimizer call is
`0x004c4430`, which dispatches on the exact optimization-level byte:

- level 2 is implemented directly in `0x004c4430`;
- level 3 calls `0x004c4910`;
- level 4 calls `0x004c4530`.

These assignments and the sequences below are **confirmed** from the stock
binary's branches, calls, and adjacent trace strings. “Value numbering” is the
binary's wording; it belongs to the common-subexpression-elimination family.

The exact universal PCode boundary before this dispatcher is `0x00435b04`,
after initial cleanup calls at `0x0049d0f0` and `0x0049d0b0`. The optimizer call
is made at `0x00435b17`. Both the optimized and diagnostic-only paths reconverge
at `0x00435b39`, the exact post-optimizer boundary. These addresses are
**confirmed** statically and by an end-to-end stock-binary capture. The
`INITIAL CODE` print path is conditional and must not be used as the boundary.

### Level 2

1. common-subexpression elimination, mode 1
2. copy propagation, mode 1
3. add propagation

### Level 3

1. value numbering/CSE, mode 0
2. copy propagation, mode 0
3. add propagation
4. loop code motion
5. loop strength reduction
6. copy propagation, mode 1
7. loop transformations
8. copy propagation, mode 1
9. add propagation
10. copy propagation, mode 1
11. constant propagation
12. load deletion
13. add propagation
14. value numbering/CSE, mode 1
15. copy propagation, mode 1

### Level 4

1. value numbering/CSE, mode 0
2. copy propagation, mode 0
3. add propagation
4. loop code motion
5. loop strength reduction
6. copy propagation, mode 1
7. loop transformations
8. copy propagation, mode 1
9. add propagation
10. copy propagation, mode 1
11. constant propagation
12. load deletion
13. copy propagation, mode 1
14. add propagation
15. array-to-register transformation
16. constant propagation
17. copy propagation, mode 1
18. value numbering/CSE, mode 1
19. copy propagation, mode 1
20. vector-array conversion
21. copy propagation, mode 0
22. copy propagation, mode 1
23. second code-motion round
24. third value-numbering/CSE round
25. copy propagation, mode 1

The O4 vector-array conversion is present in this target but absent from the
older GC/1.1 breakpoint table used as an external reference. It is a useful
warning against transplanting a nearby version's pass list.

### Shared backend tail

After the level-specific optimizer, `CodeGen_Generator` runs the shared stages
whose exact diagnostic boundaries are:

1. initial PCode
2. optional instruction scheduling
3. forward peephole optimization
4. register coloring at `0x004cdef0`, including spill retries
5. EABI prologue/epilogue generation at `0x004abe90`
6. prologue/epilogue merge, with `0x004aba30` in the path
7. final peephole optimization
8. optional final instruction scheduling
9. final PCode and emission

The stage ordering is confirmed. Some working function names inside each stage
remain semantic inferences and are labeled that way in
`config/GC_1_2_5/subsystems.json`.

The exact post-scheduler boundary is `0x00435baf`; the exact post-forward-
peephole, pre-allocation boundary is `0x00435bfd`. A focused CursorThink capture
showed that the scheduler removed 30 PCode instructions and added none, while
forward peephole changed 20 existing instructions without adding or removing
any. Twenty live instructions lacking normal construction events were already
present at the post-O4 boundary. They come from a direct optimizer allocation
or clone path, not either shared-tail pass.

Clone tracing resolves that path to `PCode_CloneInstruction` at `0x0049d270`.
For the focused CursorThink capture, its live calls originate at `0x0052aa93`
and `0x0052ab71` in helper `0x0052a200`, below the confirmed loop-
transformation entry at `0x005289b0`. The five-instruction loop body is copied
four times, so this is direct evidence of loop-unrolling ancestry rather than a
name inferred only from the pass string.
