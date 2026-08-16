# Optimization pipeline

Status legend: **confirmed** means observed in the stock GC/1.2.5 executable;
**reference** means suggested by another version/tool and awaiting exact-binary
verification.

## Frontend

The executable contains `IrOptimizer.c` and trace strings naming its stages.
These names are **confirmed**. The dispatcher is `IRO_Optimizer` at
`0x0042cd10`; its bytes are unchanged in GC/1.2.5n. RootCubed's independently
versioned GC/1.2.5 debugger breakpoint at `0x0042cd1f` corroborates the
identification.

### Recovering the pass-to-address mapping

`IRO_Optimizer` emits its stage names through the dump routine at `0x0044d830`,
called as `PUSH 0; PUSH <string>; CALL 0x0044d830`. The dispatcher runs each
pass, ORs the returned change flag into a running accumulator, then names the
pass it just ran. Three independent readings of that structure agree:

1. **Bracketed pairs.** `Before X` / `After X` traces enclose exactly the calls
   belonging to `X`, which identifies `IRO_LoopUnroller`,
   `IRO_CopyAndConstantPropagation`, and `RewriteBitFieldTemps` unambiguously.
2. **The enumerating string.** `Second pass:IRO_CopyAndConstantPropagation,`
   `IRO_ConstantFolding,IRO_EvaluateConditionals` is preceded by exactly three
   calls in that order, fixing all three addresses at once and independently
   reproducing the `IRO_CopyAndConstantPropagation` address from (1).
3. **Self-naming trace.** `IRO_FindLoops` prints
   `IRO_FindLoops:Found loop with header %d\n` from inside its own body.

Passes reached from more than one call site (`IRO_RemoveUnreachable`,
`IRO_RemoveRedundantJumps`, `IRO_RemoveLabels`, `IRO_UseDef`,
`IRO_ConstantFolding`, `IRO_EvaluateConditionals`, `IRO_BuildflowGraph`) resolve
to the same address at every site.

### Confirmed pass order and addresses

| Order | Pass | Address |
| --- | --- | --- |
| 1 | `IRO_BuildflowGraph` | `0x00449e30` |
| 2 | `IRO_EvaluateConditionals` | `0x00455930` |
| 3 | `IRO_RemoveUnreachable` | `0x00456860` |
| 4 | `IRO_RemoveRedundantJumps` | `0x00456670` |
| 5 | `IRO_RemoveLabels` | `0x00456620` |
| 6 | `IRO_BuildflowGraph` (second build) | `0x00449e30` |
| 7 | `IRO_ScalarizeClassDataMembers` | `0x0044ab00` |
| 8 | `IRO_CopyAndConstantPropagation` | `0x00458970` |
| 9 | copy/constant-propagation companion | `0x004582f0` |
| 10 | `IRO_RangePropagateInFNode` | `0x00456ba0` |
| 11 | `IRO_ExpressionPropagation` | `0x0042c9d0` |
| 12 | `IRO_UseDef` | `0x00459b30` |
| 13 | `IRO_ConstantFolding` | `0x00455a70` |
| 14 | `IRO_LoopUnroller` | `0x0045fa80` |
| 15 | `IRO_FindLoops` | `0x00461040` |
| 16 | post-`FindLoops` loop pass (unnamed) | `0x00461360` |
| 17 | second pass: copy/const prop, folding, conditionals | `0x00458970`, `0x00455a70`, `0x00455930` |
| 18 | `IRO_CommonSubs` | `0x0044df00` |
| 19 | `IRO_DoJumpChaining` | `0x00456a60` |
| 20 | `RewriteBitFieldTemps` | `0x0044ade0` |

Every stage is gated on an option byte in the `0x005842xx` block, so the
sequence above is the maximal path rather than an unconditional one.
`IRO_LoopUnroller` at `0x0045fa80` is a 21-byte wrapper: it sets the global at
`0x0058800c` to 1, calls the unroller body at `0x0045f7c0`, then the shared
teardown at `0x0044bb40`.

`IRO_FindLoops` and the unnamed pass at `0x00461360` are both bracketed by the
`Before/After IRO_FindLoops` traces, with the flag byte at `0x0057f6b5` set
between them. `0x00461360` is therefore loop-structure work running immediately
after loop discovery.

### Loop discovery and unroll candidate selection

`IRO_LoopUnroller`'s body is `IRO_FindLoops_Unroll` at `0x0045f7c0`, named by
its own trace `IRO_FindLoops_Unroll:Found loop with header %d\n` — a separate
loop finder from `IRO_FindLoops`, which prints `IRO_FindLoops:Found loop with
header %d\n`. `IRO_FindLoops_Unroll` runs in three phases:

1. **Back-edge search.** It walks every flow node (list threaded through
   `+0x32`) and each node's predecessor array (`+0x0a`, count at `+0x08`),
   testing the predecessor's dominator bit vector at `+0x2e` for the header's
   index. A hit is a back edge. Out-of-range bit indices trap through the
   `BitVector.h` assertion at `0x00445780`.
2. **Loop record construction.** The first back edge per header seeds the
   scratch bit vector at `0x005880dc` with the header and latch blocks, then
   collects the loop body through the self-recursive walker at `0x004614f0`.
   Each loop gets an `0x12`-byte record from the arena at `0x00441f20`, pushed
   onto the list headed at `0x00587104`.
3. **Innermost selection.** Every pair of loop records is compared with the bit
   vector predicate at `0x00462a20`; when it holds, the containing loop's flag
   bit 0 is cleared (`AND byte ptr [ESI],0xfe`).

The final walk then visits only records that still have bit 0 set — the
innermost loops — and for each calls `0x0045cea0` with the sign-extended option
byte at `0x005842f0` and the loop's header node, followed by
`IRO_ExpressionPropagation`. `0x0045cea0` is therefore the unroll worker and
`0x005842f0` its controlling option; neither is reconstructed yet.

Loop record layout, as used by this function:

| Offset | Field |
| --- | --- |
| `+0x00` | flag byte; bit 0 = still an unroll candidate (innermost) |
| `+0x02` | pointer to the loop's block bit vector |
| `+0x06` | next loop record |
| `+0x0a` | header flow node |
| `+0x0e` | initialized to 0; purpose not yet established |

Total size `0x12`. The bit vector helpers around it — `0x00462c60` (allocate or
resize to a bit count), `0x00462a80` (clear), `0x00462aa0` (copy), and
`0x00462a20` (the containment predicate) — are named from their call shapes and
remain **inferred**.

### The unroll worker and its rejection rules

`0x0045cea0` is 10,124 bytes and belongs to **`IroLoop.c`**, confirmed by the
filename it passes to the assertion helper. It carries five diagnostic strings,
which name the decisions directly:

- `No predecessor outside the loop`
- `while(n--) loop`
- `loop not unrolled because induction used in loop`
- `loop not unrolled because loop has multiple exits`
- `Could not find loop with and induction with MOD and DIV operation`

The control flow around them recovers three loop-descriptor flag bits and the
shape of the decision. A descriptor word is tested for `0x10000`; when clear,
control transfers to the main unroll path at `0x0045eb60`. When set, the pass
prints `while(n--) loop` and takes a specialized path that then rejects on a
second descriptor word:

| Flag | Meaning | Effect |
| --- | --- | --- |
| `0x10000` | `while(n--)`-shaped loop | selects the specialized path |
| `0x0800` | induction variable used inside the loop | rejects the unroll |
| `0x1000` | loop has multiple exits | rejects the unroll |

After both rejections it walks an induction-variable list (next pointer at
`+0x18`), looking for an entry whose flag word has bits `0x1` and `0x2` set —
the MOD/DIV induction the last message names — and gives up when none is found.

The bulk of the worker, including the main unroll path at `0x0045eb60`, is not
yet reconstructed.

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
2. optional instruction scheduling (exact algorithm and machine model in
   `SCHEDULER.md`; validated simulator in `(case study withheld)`)
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
