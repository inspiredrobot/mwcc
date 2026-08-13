# Allocator replay casebook

## Target

Allocator replay v1 turns the reconstructed register allocator into an
explanation tool for real Melee matching work. Its input is a pre-coloring
PCode snapshot from a sandboxed GC/1.2.5 compilation. Its output must include:

- each block's `use`, `def`, `live_in`, and `live_out` sets;
- every interference edge and the instruction or ABI seed that created it;
- each eligible or rejected copy-coalescing decision;
- simplify-stack order and spill-cost/degree ranking;
- available-color masks and the chosen physical register;
- final coalesced roots and PCode operand rewrites.

The model already implements the ordinary path from liveness initialization
through assignment commit. The next tooling boundary is a static or safely
sandboxed exporter for the PCode state immediately before register coloring.
The snapshot must retain block order, successor edges, execution weights,
instruction opcodes and flags, 12-byte operand records, virtual-register
counts, class ranges, and object bindings. A source experiment is not counted
as model-backed unless its prediction is written down before recompilation.

`tools/allocator_snapshot.py` implements the version-pinned memory reader and
snapshot validator. `tools/gdb/allocator_snapshot.py` registers a
`mwcc-snapshot PATH` command for a GDB session stopped at
`Coloring_AllocateRegisters` (`0x004cdef0`). The wrapper reads the function
pointer from the first stack argument and emits raw PCode without requiring
mnemonic-table guesses. Source it only inside the offline emulator sandbox;
the command does not make executing the compiler on the host acceptable.

The same wrapper registers `mwcc-coloring-snapshot PATH` for a session stopped
at `Coloring_SelectColors` (`0x004ce2d0`). That snapshot captures every graph
node, object pointer, spill cost, current degree and color, flags, complete
neighbor list, and the simplify-stack order passed into color selection. Taking
both snapshots around one allocation pass gives us the replay input and the
exact compiler result needed to find the first divergent state transition.
Use `python3 tools/compare_coloring_snapshots.py before.json after.json` to
report virtual registers added or removed, object-binding and graph-field
changes, color changes, and movement in simplify-stack order between source
variants.

`tools/allocator_provenance.py` joins these captures into the flat
`mwcc-allocator-provenance-v1` schema. It assigns stable IDs to blocks,
instructions, operands, virtual registers, coloring nodes, interference edges,
simplify positions, coalesces, and object bindings. The old allocator operand
encoding is decoded as a signed and unsigned 32-bit value at `+0x02` and an
object pointer at `+0x06`, while preserving the original 12 bytes.

The wrapper also registers `mwcc-auto-capture DIRECTORY`. Invoke it before
continuing the compiler to write indexed initial, optimized, and pre-coloring
PCode snapshots for every compiled function, plus GPR, FPR, and vector coloring
snapshots immediately before and after each color-selection attempt. The
post-selection breakpoint comes from the x86 return address at `[esp]`; this
avoids relying on debug symbols or GDB unwinding. Capture indices follow emitted
function order and can be correlated with `powerpc-eabi-nm -n` on the output
object.

For large translation units, `mwcc-auto-capture DIRECTORY FUNCTION_INDEX`
keeps the stage, creation, allocator, and all-class coloring output for only one
emitted function. `FUNCTION_INDEX` may instead be an exact symbol name. The
debugger follows the same cached CMangler-record path as target routine
`0x004c2560`, records the resolved identity in every artifact, and filters at
the CodeGen boundary. A final `ninji` argument selects the verified Melee
GC/1.2.5n identity; the default is stock GC/1.2.5. The address set used here is
confirmed unchanged between those two binaries, but their hashes remain
distinct in every capture.

Auto-capture also traces the two PCode construction wrappers at `0x004a25d0`
and `0x004a2620` through the common builder return at `0x004a2b6d`. Every event
records the allocated instruction, creation epoch, immediate x86 callsite, and
the current-CodeGen-item pointer and header from `0x00587130`. New captures
retain all 26 recovered header bytes and decode expression fields for item
kinds 4 through 15. Expression kind `0x38` now supplies a direct, binary-backed
join to its `CompilerObject`; older captures retain their original 18-byte raw
headers and remain usable.

Each creation operand with a nonzero compiler-object pointer also retains the
object tag/kind, type pointer, object flags, and the referenced type's
kind/size/flags/subtype. These fields are the exact inputs used by the recovered
`m`, `M`, and `l` format branches, so memory/immediate and access-flag decisions
can be explained without guessing an AST name.

On hosts where a `linux/386` container is emulated, host `ptrace` may be
unavailable. `tools/docker/Dockerfile.debugger` provides native GDB,
`gdb-multiarch`, and `qemu-user`. Run the verified i386 Wibo binary under
`qemu-i386 -g PORT`, then connect GDB to QEMU's built-in stub inside the same
network-disabled container. This path observes the emulated process without
host `ptrace`. Runtime hardening remains mandatory: no network, a read-only
root, no capabilities, `no-new-privileges`, resource limits, tmpfs scratch,
and read-only compiler/input mounts.

Concrete capture recipe (verified on an arm64 host against a Melee TU). Build
`mwcc-debugger:arm64` from `Dockerfile.debugger`, write a GDB script that does
`target remote :1234`, `source /mwcc/tools/gdb/allocator_snapshot.py`,
`mwcc-auto-capture /capture`, `continue`, then run one hardened container that
launches the compiler under the stub and attaches GDB in the same namespace:

```sh
docker run --rm --platform linux/arm64 --network none --read-only \
  --cap-drop ALL --security-opt no-new-privileges --tmpfs /tmp:rw,nosuid \
  -e HOME=/tmp -e WIBO_TMP_DIR=/tmp \
  -v $MELEE:/basebuild:ro -v $WORKTREE:/melee:ro -v $MWCC:/mwcc:ro \
  -v $CAP:/capture:rw -v $CAP/capture.gdb:/capture.gdb:ro -w /melee \
  mwcc-debugger:arm64 /bin/sh -c \
  "qemu-i386 -g 1234 /basebuild/build/wibo_old \
     /basebuild/build/compilers/GC/1.2.5n/mwcceppc.exe <exact TU cflags> \
     -c src/melee/<tu>.c -o /capture/<tu>.o & \
   gdb-multiarch -batch -x /capture.gdb; wait \$!"
```

The stock `1.2.5` and patched `1.2.5n` compilers share the coloring addresses
(`0x004cdef0`, `0x004ce2d0`), so a TU can be captured with whichever compiler
produced the object under study; use `1.2.5n` when its output diverges from
stock for the function of interest (e.g. `fn_80262648`).

The initial snapshot deliberately stops short of object names and class ranges.
Those fields will be added after their exact target layouts are recovered. The
raw 12-byte operand encoding and decoded object pointer are retained so
snapshots taken now can be enriched later without rerunning the compiler.

The first Melee validation showed that this boundary is required, not merely
convenient. Two sources can reach coloring with equivalent long-lived address
webs while differing in the compiler objects that created them. Replay v1 must
therefore preserve object bindings and virtual-register creation order. A
separate pre-PCode trace is needed for string pooling, declaration lowering,
dead-definition removal, and scalar replacement; the coloring snapshot alone
cannot explain changes that have already happened in those stages.

### The built-in per-pass IR dumper is a `ret` stub (do not re-attempt)

The compiler contains a complete per-pass IR/asm dump scaffold, but the actual
dump routine is stubbed out in the shipped release build, so it produces no
output. Verified against `GC/1.2.5n` (region byte-identical to stock `1.2.5`),
2026-08-02:

- `gCOptimizerDumpEnabled` is the gate byte at `0x00584226` (`.bss`, so it is
  not file-patchable; it defaults to 0). Its **only** writer is the single
  `mov byte [0x00584226], 0` initializer at `0x0042c8db` (immediate at file
  offset `0x2bce1` in the `1.2.5n` image). No code path ever sets it to 1, so
  there is no command-line flag or environment variable to enable dumping.
- Every stage label exists as a real string: `BEFORE GLOBAL OPTIMIZATION`,
  `AFTER CODE MOTION` (VA `0x00562060` in stock), `AFTER STRENGTH REDUCTION`,
  `AFTER REGISTER COLORING` (`0x0054fbd4`), etc.
  They are referenced by live `push str; push name; call COptimizer_Dump`
  sequences inside `COptimizer_Level4` (`0x004c4620`), `COptimizer_Level3`
  (`0x004c4a00`), and the coloring path in `CodeGen_Generator` (`0x00435c0e`).
- All of those call sites target `COptimizer_Dump` at `0x004c4bd0`, whose body
  is a single `c3` (`ret`) followed by alignment padding. The dump
  implementation was compiled out; the call setup is dead code. `Coloring.c`'s
  `Coloring_Dump` reference resolves to the same stub.

Consequence: you **cannot** observe the loop-code-motion / LICM hoist (or any
other pre-coloring pass boundary) by flipping the gate byte and reading the
compiler's stdout — enabling it via GDB or a one-byte patch of the `0x0042c8db`
immediate is inert (confirmed: a trivial O4 loop function compiles cleanly and
prints nothing). The pre-PCode trace called for above must therefore come from
a memory snapshot of the IR taken at a pass boundary (e.g. a new breakpoint
after `COpt_00524bd0`, the code-motion step at `0x00524bd0`, read via the same
`allocator_snapshot` PCode reader), not from the binary's own dumper. Modelling
the LICM cost decision directly (`COpt_SetLoopCodeMotionMode` `0x00523650`,
`COpt_00521a10`, `COpt_00524bd0`) remains the only way to *predict* a hoist
rather than merely observe its result.

A real `mnCharSel` capture validates the join. Allocator capture 13 contains 44
blocks, 165 instructions, 880 operands, and 111 register records. Its before
and after GPR coloring captures contribute 218 node states and 999 unique
interference edges per phase. The resulting facts recover, for example, virtual
register 38's `LWZ` definition, two `ADD` uses, simplify position 70, object
`0x40d4c2c0`, and final color r8. Virtual registers 39, 47, and 48 likewise
retain their complete definitions, uses, simplify positions, objects, and
colors.

Compiler-generated temporaries can have object pointer zero even when their
lifetimes are decisive. Object bindings alone therefore cannot explain virtual
register birth. Auto-capture now traces both object-backed allocator calls and
direct GPR/FPR/VR counter increments. The direct sites are generated from the
verified PE by `tools/virtual_register_sites.py` and checked into per-version
catalogs; object-allocator-internal increments are excluded to prevent duplicate
origins.

That next boundary is implemented. The unconditional capture points are
`0x00435b04`, after initial cleanup and before optimizer dispatch, and
`0x00435b39`, after both optimizer/debug branches reconverge. The nearby
`INITIAL CODE` diagnostic is conditional and is not a valid universal capture
point. `tools/compare_pcode_stages.py` reports instructions added, removed,
mutated, or genuinely reordered while retaining creation callsites.
`tools/allocator_provenance.py --creations` emits `created_by` facts, and
`tools/explain_register.py` follows a virtual register through its definition
and uses, creation epoch/callsite, object binding, interference states,
simplify positions, coalescing state, and final color.

An end-to-end run against the exact stock GC/1.2.5 SHA-256 in the hardened,
network-disabled debugger sandbox validated this path. A small `-O4,p` float
function produced 18 initial instructions and creation events, 17 optimized and
allocator instructions, complete `created_by` coverage for all 17 survivors,
one dead creation, one in-place operand rewrite, and no genuine reorder. Five
nonzero opaque CodeGen-item identities were retained across the 18 emission
events; three prologue events correctly had no current item.

The focused CursorThink validation closes the original motivating question.
Emitted function 15 produced 2,507 initial, 2,365 post-optimizer, and 2,335
pre-coloring instructions. `fpr:265` is
defined by `LFD` creation `c1665` and consumed by `FCMPO` creation `c1666`.
Both are `initial_lowering` events attached to the same opaque CodeGen item of
raw kind 7; neither was created by the backend optimizer. The node has no
compiler-object binding, simplifies at position 8, and selects physical f23.
Its creation-time `LFD` destination already is virtual FPR 265 with object zero,
while its memory operand retains object `0x40fb6258`. This proves the candidate's
ninth FPR web is born during frontend-driven initial lowering and survives O4,
rather than being invented by optimization or color selection. Virtual-register
birth tracing now adds the missing cause: `fpr:265` has a unique direct origin
at `0x004a05b7` in `Operands_ForceFPR`. That `Operands.c` routine converts a
kind-9 memory operand to an FPR, allocates a destination when the requested FPR
is zero, and chooses LFS versus LFD from type size. CursorThink takes the LFD
path. Across this function the capture records 180 object-backed GPR births, 75
object-backed FPR births, 659 direct GPR-temporary births, and 243 direct
FPR-temporary births. All 695 live GPR and 273 live FPR webs now join to exactly
one birth event; there are no unexplained live allocator webs in this capture.
`tools/rank_register_origins.py` aggregates these links by register class,
allocation kind, and exact lowering site, including live/dead counts and
definition mnemonics. Its comparison mode ranks allocation and live-web deltas
between two source captures, so a source experiment can be tied to the first
changed lowering operation rather than judged only by final assembly.

The largest GPR birth cluster is now represented by the typed
`Operands_ForceGPR` model at `0x004a0ba0`. The routine selects LBZ, LHZ, LHA,
or LWZ and their indexed forms from the lowered type, materializes small
constants with LI, and splits larger constants into an adjusted LIS/ADDI pair.
At optimization levels above one, a nonzero low half can allocate a distinct
temporary for the LIS result; that increment is therefore a real lowering
choice which can change interference before allocation. Eight-byte integer
types bypass scalar normalization and delegate to the paired-GPR routine at
`0x004a0680`. These distinctions make the origin ranking actionable: a delta
at the ForceGPR site can now be classified as a memory-load destination, a
large-constant helper web, a condition-register extraction, or paired-value
lowering rather than an undifferentiated GPR increment.

The same run exposed the next boundary worth instrumenting. Of 2,337 allocator
instructions, 2,317 join to normal creation events. Stage snapshots prove that
the remaining 20 are among 67 instructions added by O4 before `0x00435b39`.
The scheduler then removes 30 instructions without adding any, and forward
peephole mutates 20 without adding or removing any. The unmatched instructions
are therefore made by an optimizer-side PCode allocation/copy path which
bypasses the normal construction wrappers, not by the shared tail. Arena-
allocation and clone-entry tracing during O4 located the path. All 20 are made
by `PCode_CloneInstruction` at `0x0049d270`, and every copy retains its exact
live parent. Sixteen clone calls come from `0x0052ab71` and four from
`0x0052aa93`, both inside helper `0x0052a200` under the confirmed loop-
transformation pass. They repeat the same five-instruction
LBZ/CMPLI/BT/ADDI/ADDI body four times. On the subsequently captured candidate
revision, all 2,335 pre-coloring instructions have provenance: 2,315 normal
surviving creations and 20 optimizer clones.

## Initial Melee cases

### `Ground_801C20E0`: stack homes and provenance-only web ordering

- Source: `src/melee/gr/ground.c`; exact Melee commit `(rev withheld)`.
- Full case and replay artifacts: `(case study withheld)`.
- Symptom: the baseline had the correct 163-instruction shape but a `0x30`
  frame instead of `0x28`, followed by a volatile-register rotation after the
  frame was corrected.
- Stack result: the baseline had two required non-leaf parameter homes plus
  removable `found` and `desc` homes. Four and three homes both aligned to
  `0x30`; only removing both locals exposed `0x28`. Inlining the boolean return
  removed `found`, while a raw descriptor dereference avoided the inline
  accessor/CSE path that recreated `desc`.
- Coloring result: K=29 replay proved the retail allocation was reachable with
  the same interference graph. Binding `flags` inside its bit test changed its
  frontend provenance/birth rank while keeping the address `addi` in place.
  Splitting one stage-data value across `dat` and `array_dat` inline parameters,
  passed as a caller local and the equivalent direct expression, moved the
  merged web between `desc` and `arr`. CSE removed the duplicate machine value,
  but distinct source origins changed creation/coalescing order. The final
  volatile clique is byte offset `r5`, `desc` `r6`, `dat` `r7`, `arr` `r8`, and
  `flags` `r9`.
- General stack lesson: aligned frame size is not a home counter. Capture the
  home list and identify owners directly; a one-home improvement can be real
  while remaining invisible in the prologue.
- General allocator lesson: semantically equal expressions are not necessarily
  allocator-identical. Inline ownership, compiler-object provenance, and
  virtual-register birth order can change coloring even when CSE restores the
  same instruction graph. Use replay first to prove rank-only reachability,
  then compare creation/provenance captures to select a source lever.
- Diagnostic warning: flattening the helper correctly moved two webs into the
  caller-local stratum, but the one-field counter carrier needed for the retail
  CTR schedule acquired a home and regrew the frame. Source experiments can be
  valuable graph probes without being viable final code, and carrier
  scalarization must be measured per site.

### `Ground_801C466C`: sequential semantic carriers and independent stack homes

- Source: `src/melee/gr/ground.c`; exact Melee commit `(rev withheld)` in PR #(withheld),
  linked as a fully matching translation unit by `(rev withheld)`.
- Symptom: the baseline already had the retail 206-instruction schedule, but
  five semantic register webs differed. The closest pre-final source reached
  99.8000% with only seven operands in the scan loop using r29 instead of
  retail r28. An apparently equivalent source could also be register-exact
  while retaining a `0x38` frame and placing an addressed `Vec3` eight bytes
  below retail.
- Inline-boundary result: flattening the stage callback scan into the caller
  moved the callback cursor from r29 to retail r26 without changing the PCode
  graph. The helper boundary had put the value in a distinct inline-local
  provenance stratum; caller-local lowering supplied the retail birth/rank.
- Sequential-carrier result: retail reuses r28 for two non-overlapping values
  with unrelated C types: first the selected `LightList**`, later the
  `HSD_AObjSetFlags` callback address. A union with `lights` and `callback`
  members gives both values one source owner. Assigning the callback member
  after copying the selected list to its traversal cursor both reproduces the
  r28 reuse and prevents selected-list-to-cursor coalescing. This restores the
  two required copy boundaries as well as the late callback operands.
- Loop-coloring result: independent scalar `i` and `count` locals let two
  non-overlapping webs reuse retail r28 across `mtctr`. Combining them in an
  eight-byte struct preserved the instruction graph and stack footprint but
  ranked their aggregate carrier at r29. This is a concrete warning that a
  convenient aggregate can impose frontend ownership even when scalar
  replacement removes all field accesses from final code.
- Stack result: scalarizing `i` and `count` made every register exact but
  removed eight bytes of local homes, shrinking the frame from `0x40` to
  `0x38` and moving `sp10` from `sp+0x10` to `sp+0x8`. Two otherwise-dead
  four-byte declarations, `var_r3` and `temp_r28`, independently own that
  missing band. Retaining them restores the retail frame and every stack
  displacement without altering the live register webs. MWCC lays these homes
  and the addressed `Vec3` out in reverse local order.
- General lesson: register ownership and stack ownership are orthogonal source
  constraints. Solve them separately. First find a scalar/source-carrier shape
  that reproduces semantic web creation, coalescing, and coloring; then account
  for every reserved local home, including dead declarations. Replacing dead
  homes with a live aggregate can preserve total frame size while silently
  changing allocator provenance and therefore cannot be treated as equivalent.
- Verification: `Ground_801C466C`, `Ground_801C20E0`, and
  `Ground_801C4FAC` all score 100.0000%; the rebuilt Ground object reports 100%
  for every allocatable section, and the linked Melee checksum passes.

### `COpt_00521bb0` host-compiler control

- Source: `src/backend/CodeMotion.c` in this MWCC reconstruction.
- Symptom: a semantically direct first draft was 347 bytes with an extra saved
  register and only 22.19% positional comparable match against the 352-byte
  retail extent.
- Result: signed `PCodeBlock +0x2e` equality and an unsigned subtract/range
  expression reproduced the target's exact flag and barrier tests, reaching
  61.74%. Removing a named `short opcode` local then reached 100% across all
  344 instruction bytes; the remaining eight target bytes are alignment.
- Mechanism: the named local extends the opcode value's source lifetime and
  makes it a separate register web. Repeating `instruction->opcode` lets copy
  propagation load the opcode into the low half of the register whose flags
  value just died. That removes one callee-saved register and reproduces the
  target allocation. Do not introduce a convenience local merely because the
  field is read repeatedly; compare the web lifetime it creates.

### `efAsync_Dispatch`

- Source: `src/melee/ef/efasync.c`
- Symptom: high register pressure plus stack-band movement across a giant
  dispatch; a one-field generator aggregate fixes a small operand cluster,
  while inline ownership moves an addressed `Vec3` through different bands.
- First question: does the aggregate change virtual-register creation,
  coalescing eligibility, or only frame ownership?
- Relevant model: `SpillCode_ConstructInterference`,
  `SpillCode_CoalesceCopies`, and `Coloring_CommitAssignments`.
- Initial prediction: snapshots of the scalar and one-field aggregate variants
  will have the same ordinary instruction operands but different virtual roots
  or copy edges around case `0x40d`; if not, the effect belongs before PCode
  allocation and becomes a virtual-register creation target.

### `lb_8001044C`

- Source: `src/melee/lb/lbspdisplay.c`
- Symptom: a declaration-with-initializer can rank a local web ahead of four
  parameter homes and rotate their saved-register assignment; `register` is
  byte-neutral at `-O4,p`.
- First question: where does declaration initialization alter virtual-register
  numbering or simplify order?
- Relevant model: `SpillCode_BuildLocalLiveness`,
  `SpillCode_ConstructInterference`, and `Coloring_SimplifyGraph`.
- Initial prediction: moving the initializer to a separate statement will not
  change the final interference graph, but will change virtual-register IDs or
  the order in which equal-degree nodes reach the simplify list.
- V0 result: confirmed, with an additional frontend dependency. On Melee
  `(rev withheld)`, replacing a synthetic concatenated `char` object with five
  separate first-use literals moved the two full-function global-address webs
  into the retail r25/r26 assignment. The function improved from 99.7345% to
  99.8053%; only one real `li` placement remained, plus three relocation-name
  artifacts whose target and candidate operands were otherwise identical.
- The contiguous retail bytes did not describe one C object. In particular,
  `!(jobj->flags & JOBJ_USE_QUATERNION)` must remain generated by `HSD_ASSERT`,
  not hand-authored into the preceding data. Restoring it inside the synthetic
  array emitted a second copy and moved the assert operand from `+0x44` to
  `+0x70`.
- An explicit typed pointer to the vector table was canonicalized and did not
  change allocation. This rejects declaration spelling as the cause; pooled
  literal/object formation changed virtual-web creation or binding before
  coloring.
- The remaining counter demonstrates a frontend scheduling/ranking conflict.
  A declaration initializer gives the counter r31 but emits its `li` before
  the early-return guards. A later assignment or nested declaration emits at
  the retail location but colors the counter r23 and rotates the saved webs.
  This compiler mode rejects mixed declarations and statements, so there is no
  same-compound C99 placement to test.
- A one-field aggregate was memory-homed rather than scalarized: it added four
  bytes to the local stack band and shifted every addressed local. A parameter
  carrier copy was eliminated before coalescing and reproduced the late,
  low-ranked counter allocation. These observations agree with the recovered
  dead-definition-before-coalescing order.

### `it_8026C75C`

- Source: `src/melee/it/itspawn.c`
- Symptom: the table pointer and the `chk2` flag were exchanged between r28
  and r29, with every opcode and lifetime boundary otherwise identical.
- Prediction: introducing a source object for the incoming table pointer will
  change the coalescing root or equal-degree insertion order without retaining
  a copy instruction. Once that object exists, declaration order should
  control the three-way clique formed by the table, saved weight, and flag.
- Result: confirmed at Melee commit `(rev withheld)`. `ItemPickTable* tbl = table`
  moved the table into retail r28 and raised the function from 96.2500% to
  99.6710%. Declaring the remaining long-lived objects in `saved`, `chk2`,
  `tbl` order selected retail r30, r29, r28 and produced a 100.0000% match.
- Moving `tbl` ahead of the scalar declarations rotated the same clique to
  r30, r29, r28 in declaration order. Merely exchanging two uninitialized
  boolean declarations was neutral. The initializer-bearing pointer object,
  not textual declaration order by itself, is the frontend boundary.
- Changing `saved` from `int` to the superficially natural `u16` erased the
  pointer-alias effect and restored the original r28/r29 swap. Replay input
  therefore needs source-object/type bindings in addition to the final
  interference graph: equal emitted integer values are not sufficient.

### `lbRefract_800222A4`

- Source: `src/melee/lb/lbrefract.c`
- Symptom: retail assigns the data-section base and three zero-derived loop
  offsets to r30, r29, r28, r31; the reconstruction rotates them to r31, r30,
  r29, r28 while preserving the instruction stream.
- Counter declaration order, nested versus function scope, and a declaration
  initializer were neutral. Giving the initial count clear and `j` the same
  explicit source owner also canonicalized to the baseline.
- A named `imagedesc0` pointer survived as an extra saved register. A typed
  pointer to the global state allowed field-pointer folding, removed three
  retail saved webs, and shrank the frame. Neither represents the retail
  frontend shape.
- Replacing `i` and `j` initialization with a chained assignment made `j`
  opaque and emitted a real `mulli` for a provably zero induction value. This
  independently confirms the const-propagation-opacity rule from
  `ft_800C85B8`, but rejects it for this function.
- Named static arrays for the two archive strings preserved `.data` at 100%
  and were allocation-neutral. Here the rotation is not caused by anonymous
  versus named string objects, unlike `lb_8001044C`.

### `mnCharSel_8025FDEC`

- Source: `src/melee/mn/mncharsel.c`
- Symptom: the stream, frame, graph, and saved registers matched, but four
  volatile GPR webs formed a 16-word r5/r7/r8 cycle.
- The stock GC/1.2.5 and patched Melee GC/1.2.5n compilers emit identical
  170-word output for the baseline, validating the stock-address capture.
  `mnCharSel_8025FDEC` is allocation index 13 in this translation unit.
- Captured candidate webs were `v38` (`css` to r8), `v48` (player stride to
  r5), `v47` (normalized door to r7), and `v39` (icon index to r8). Replaying
  the recovered lowest-color selection reproduced every compiler assignment.
- Permuting only those nodes proved that moving `v38` ahead of `v48` in select
  order yields retail r5/r7/r8/r7 and leaves every other color unchanged. No
  interference or coalescing edge was missing.
- Result: a one-field aggregate around the `CSSData*` owner changes exactly
  that frontend/simplify ordering. Declaring it before the addressed `sp10`
  local also accounts for MWCC's reverse stack-band order, keeping `sp10` at
  `0x10` while the carrier occupies the existing `0x14` hole. The function is
  100.0000% (170/170 words). Declaring it after `sp10` leaves only three
  `0x10`/`0x14` displacement differences; adding `icon_idx` as a second field
  grows the frame and rotates another web. This is an order-only allocator
  case, not evidence for a larger aggregate.

### `fn_80262648`

- Source: `src/melee/mn/mncharsel.c` (Melee `(branch withheld)`, PR #(withheld),
  compiled with the target `GC/1.2.5n`; capture indices below are from that
  compiler, not stock).
- Symptom: instruction-identical to retail (every GPR difference is
  `ARG_MISMATCH`), 98.66% under `functionRelocDiffs=data_value`. Three
  callee-saved webs form a 3-cycle: the `&mnCharSel_803F0A48` (`icons`) base
  is retail r31 / candidate r30; the loop-local re-materialized
  `mnCharSel_804A0BD0` base is retail r27 / candidate r31; `n_doors` is retail
  r30 / candidate r27. An independent `f22`/`f23` FPR swap and an
  `HSD_AObjReqAnim` `@ha`-temp scheduling difference account for the rest.
- Capture: allocation index 16 (nm `-n` text order; `mnCharSel_8025FDEC` is 13,
  matching the earlier calibration). One GPR coloring attempt.
- **Derived GPR color model (validated).** `Coloring_GPRColorMask` and
  `Coloring_ClaimGPRColor` are still `extern` stubs, but the captured
  before/after pairs pin their behavior: the initial `color_mask` is the
  volatiles `{r0, r3..r12}`; callee-saved registers are *claimed on demand
  high-to-low* (`r31, r30, ..., r14`) and OR'd into the mask; `SelectColors`
  otherwise takes the lowest set bit of `available`. Replaying `SelectColors`
  with this mask over the captured `simplify_order` reproduces the compiler's
  colors exactly for 24 of 25 functions in this TU (the 25th differs only on
  coalesced roots with color >= 32, which the plain replay does not resolve).
- Prediction: because the stream is identical, only select order can move the
  three webs. A brute-force over the early select slots found 24 permutations
  that yield retail r31/r27/r30 for the three webs; every one selects the
  `icons` base first and `n_doors` second, ahead of the re-materialized
  `804A0BD0` base. So the residual is a reachable ordering, not a missing
  interference/coalescing edge.
- Result: rejected as source-fixable. The three rotating webs are all
  compiler-materialized bases (two global-address materializations plus the
  `n_doors` count); no nameable local owns them. Declaration-order and type
  sweeps of the real locals (`n_doors` position/`s32`/one-field carrier,
  `css`-first, `prev_port` widening, swapping the two loop walker
  declarations) never fell below the baseline and never flipped the base-web
  order. An explicit `CSSIcon* icon_tbl = icons` regressed to 96.35% by
  changing the addressing mode. This is the same terminal callee-saved
  permutation class as `grCorneria_801E25C4`: the ordering is set by
  `SimplifyGraph` dynamics over equal-spill-cost compiler webs, which
  instruction-identical source edits cannot steer.

### `mnCharSel_CursorThink`, `fn_802640A0`, `fn_802633B0`

Same TU/PR (#(withheld)); all instruction-identical to retail (only `ARG_MISMATCH`
plus a few scheduling `INSERT`/`DELETE` pairs). The GPR model reproduces each
one's colors exactly, and the FPR model (below) likewise. They fall into two
non-source-steerable classes:

- **Extra-callee-saved (register-pressure) class — `CursorThink` (95.15%).**
  C saves one *more* callee register than retail in *both* files: GPR `stmw
  r18` vs retail `r19`, and FPR `f23..f31` (9) vs retail `f24..f31` (8). The
  extra `f23` band and `r18` band are then reused by many short webs, so the
  single extra live value at peak pressure cascades into ~530 GPR + ~130 FPR
  `ARG_MISMATCH` rows. The FPR 9-clique bottleneck is `vr265`, a compiler
  temp (`object=0`) holding the `804DC4F0` double constant — but retail also
  keeps that constant callee-saved (its `f26`), so the surplus is one extra
  simultaneously-live FP value at peak, not that constant per se. This is the
  documented "anchor split / -2.2f hoist" residual; it is a frontend
  pressure/lowering difference, and prior spelling-unification attempts (see
  the Melee session notes) failed. Not a select-order case.
- **Same-span permutation class — `802640A0` (97.26%), `802633B0` (98.36%).**
  Both use the identical callee-saved span as retail (no extra register) but
  permute it. `802640A0` is a broad many-to-many reallocation (`C r18` maps to
  five different retail registers across regions), not a clean clique rotation.
  `802633B0` is closer to nameable: its inner row-render loop rotates
  `name_color`/`used_name_color` (`GXColor*` aliases) against the `j`/`page_off`
  counters, and retail ranks the initializer-bearing pointer aliases first
  (the it_8026C75C lever). But declaration-order sweeps of those five inner
  locals moved the score only +0.012pp (98.3578 -> 98.3701), because the clique
  is entangled with an outer `r22`/`r23` swap and a swapped pair of int->float
  conversion stack temps (`0x90`/`0x98`) that are compiler-owned. No coordinated
  stream-preserving edit closes it.

**Validated FPR color model.** Identical shape to the GPR model: initial mask =
volatiles `{f0..f13}`; callee-saved claimed high-to-low `f31..f14`; lowest set
bit otherwise. Reproduces CursorThink's FPR colors exactly (0/310 mismatches).
The stock auto-capture only records `reg_class==0`; capturing FPR needs the
`ColoringBreakpoint` filter widened to `reg_class in (0,1)` and the snapshot
filename parameterized by class (`-gpr-`/`-fpr-`). Worth folding into
`tools/gdb/allocator_snapshot.py`.

### `fn_8016E2BC`

- Source: `src/melee/gm/gm_16AE.c`
- Symptom: the single-player `is_teams` result colored to r24 instead of r27,
  and the loop's final flag load folded from retail `addi` plus `lbzx` into an
  equivalent `lbz` through a running pointer.
- Prediction: the single-player and multiplayer values are mutually exclusive
  source objects even though the reconstruction reused one local. Splitting
  them should let the first branch coalesce with the later r27 constant web
  without changing emitted instructions.
- Result: confirmed at Melee commit `(rev withheld)`. A separate
  `single_is_teams` local removed the complete r27/r24 mismatch and raised the
  function from 99.1192% to 99.1710%. Only the two-instruction access-form
  residual remains.
- A typed two-argument load-only inline was frame-neutral but canonicalized to
  the same running-pointer load. A helper-local base pointer billed eight
  bytes and shifted both addressed vectors. A function-scope base alias
  survived as an extra saved register. A synchronized second scalar index also
  survived and rotated the entire saved set. These reject allocator-only
  explanations for the remaining rows: the open boundary is add propagation
  across the inlined `getSpawnPoint` pointer web.

### `grCorneria_801E25C4`

- Source: `src/melee/gr/grcorneria.c`
- Symptom: the remaining difference is a five-web callee-saved permutation;
  broad declaration, alias, and inline-boundary sweeps preserved the opcode
  stream but failed to satisfy both the preheader scratch register and later
  call-argument reuse.
- First question: which exact copy edge or simplify tie orders the five webs?
- Relevant model: `SpillCode_CoalesceCopies`,
  `Coloring_SimplifyGraph`, and `Coloring_SelectColors`.
- Initial prediction: the flat and wide-inline variants will produce the same
  interference subgraph but different coalescing roots; if roots are identical,
  virtual-register insertion order is the deciding state.

### `fn_80190ABC`

- Source: `src/melee/gm/gmtou_0.c`
- Symptom: one case-5 pointer-register boundary remains after typed table
  reconstruction and access-form cleanup.
- First question: is the pointer swap caused by a last-use boundary, copy
  coalescing, or an equal-degree coloring decision?
- Relevant model: `SpillCode_MarkLastUses`,
  `SpillCode_CoalesceCopies`, and `Coloring_SelectColors`.
- Initial prediction: the target-shaped access form keeps one pointer live
  across the case-5 call and therefore adds exactly one interference edge;
  variants with an identical edge set should receive identical colors.
- Calibration result: current Melee upstream matches this function 100%. The
  matching source shares one pointer web across cases 5 and 6, reuses the
  earlier table local for the second case-5 base, and keeps the state base live
  through the end of that case. Keep it as a solved lifetime/coalescing fixture
  rather than an open replay target.

### `psDispParticles`

- Source: `src/sysdolphin/baselib/psdisp.c`
- Symptom: very high GPR/FPR pressure, selective memory homes, large inline
  stack bands, and broad register permutations around particle emission paths.
- First question: which homes are actual spill decisions and which were
  created earlier by inline-local ownership or aggregate lowering?
- Relevant model: `SpillCode_ComputeSpillCosts`,
  `Coloring_SimplifyGraph`, `Coloring_SelectColors`, and the pending frame model.
- Initial prediction: an actual allocator spill will appear as a colorless node
  selected by minimum `spill_cost / degree`; a stack local absent from that
  spill set must have been memory-homed before graph coloring.

## Experiment record

Each case accumulates entries with this minimum schema:

| Field | Meaning |
| --- | --- |
| Source revision | Melee commit and compiler configuration |
| Mismatch slice | Target and candidate address/instruction range |
| Stage claim | Optimizer, allocator, frame, selector, or scheduler |
| Compiler evidence | Exact recovered function and state transition |
| Prediction | Expected graph, root, color, frame, or instruction change |
| Source change | One focused experiment |
| Observation | Object diff plus captured compiler-state delta |
| Result | Confirmed, rejected, or inconclusive |

Rejected predictions remain in the record. They constrain the compiler model
and prevent later agents from repeating a source-shape sweep without a new
mechanistic reason.

## Case: melee vi1201v1 `un_8031FD18_OnEnter` — simplify dynamics recovered; residual bounded (2026-08-02)

508-instruction scene-setup function, pure GPR callee-saved permutation at
99.40%. Five captures across five source structures (`(case study withheld)`,
cap0=original single-gobj source, cap=per-block locals, cap2=decl reorder,
cap3=camera+koopa inline helpers, cap4=cap3+head helper) produced two major
new results, both replay-validated exactly:

### Simplify stack construction is now fully modeled *(confirmed, 5/5 captures exact)*

The select stack order is reproduced bit-exactly by:

- **Pass-based ascending scans**: repeatedly scan live nodes in ascending
  virtual-register order, removing (pushing) any node whose current degree
  (`len(neighbors)`, physical neighbors included, physicals never removed) is
  `< 29`; a node skipped in a pass is only revisited on the NEXT full pass.
- **k = 29** for GPR (allocatable registers: 32 minus r1, r2, r13).
- **Jam-break**: when a full pass makes no progress, remove the
  lowest-numbered remaining node (spill costs were all zero at this capture
  boundary, so cost/degree ranking degenerates to list order).

Consequence: pop order = [never-eligible core, descending vreg] ++
[everything else, descending vreg]. Combined with the select model (lowest
available bit, claims r31 down), colors are a pure function of the
interference graph and the virtual-register NUMBERING.

### Web numbering laws *(inferred, consistent across all 5 captures)*

- vr32 = the parameter-copy web.
- User decl webs: **reverse declaration order** starting at 33 (one slot per
  variable; a variable's first web takes the decl slot).
- Extra webs of a multiply-assigned variable: after all decl slots, grouped
  by variable in reverse decl order, within a variable by creation order.
- Inline-helper locals occupy a mid region (~44-57), helpers in **reverse
  call order**; single-block volatile temps number forward by block from ~66;
  optimizer-created webs (string-pool base, CSE'd zero, LICM-hoisted call
  targets, strength-reduction IVs) number in a transform-order tail region.
- Coalescing keeps the **minimum** id of the family as root (confirmed:
  koopa jobj family {47,48,49,51} rooted at 47 in cap4).

### Search results for this case

Free-numbering annealing over the 19 callee-saved webs reaches retail 19/19
on the cap3/cap4 graphs (many solutions collected; all require the string
base numbered into the decl region, the loop IV numbered above the LICM
webs, and the koopa gobj below the koopa jobj root). Source-reachable
numberings (decl permutations x variable sharing partitions x helper decl
swaps) cap at 11/19 on those graphs and 10/19 on cap0-2. On cap0-2 graphs
even free numbering caps at 18/19, so the inline-helper structures changed
the interference graph itself (pre-RA PCode 511 vs 508 instructions with an
identical final stream — post-coloring cleanup hides pre-RA differences, so
graph identity does NOT follow from final-stream identity).

Structural sweep results (all stream-identical): camera+koopa helpers are
the best found (99.52%, camera/koopa gobj webs + stand/fog blocks now
retail); flat loop (no SetupScene inline) breaks the stream head (early
string-base hoist + split zero web); a vi1101-style `char* data` user
pointer to the string pool drops a callee-saved register; helper params and
decl swaps regress or are neutral. Residual: 43 operand rows over the
head/loop cluster {zero, char_index, cobj, IV, JObjCallback base, fn base,
loop gobj, koopa jobj}. The required numbering placements (IV above the
LICM webs, string base below the helper regions) have no discovered source
mechanism; candidate next levers are optimizer-pass-order sensitivity of
the hoists (first vs second code-motion round) and helper-boundary
variations not yet enumerated. Search scripts and captures in
`(case study withheld)`.

## Numbering-region law + fixability diagnostic (2026-08-06, melee close-residual sweep)

Extends the vi1201v1 numbering laws with a general vreg-REGION map, derived by
mapping first-def block/opcode + coloring `object` for every vreg across
mnDataDel_8024FE4C (allocator-0013) and ftCo_800A8940 (allocator-0088):

- **32** = the fighter/first param.
- **33-40** = DIRECT named locals, reverse declaration order. Only these are
  steerable by C declaration order.
- **41-50** = named COPIES / derived values (a rename of a local, a field-base
  pointer, a call-result copy). Numbered by CREATION position (block/expr
  order), NOT declaration. Decl-order sweeps are inert on these.
- **52-85+** = fresh expression temps and strength-reduced induction variables.
  Numbered by FORWARD block order, multiple per block (a loop's SR walker lands
  in that loop's block; the more setup blocks precede the loop, the higher its
  number).

Coloring = descending-vreg select for non-jammed nodes (jam threshold degree
>=29, jam-core colored first). So HIGHER vreg claims a callee-saved reg FIRST.

### Diagnostic for a callee-saved swap {webX, webY} (classify by vreg + object):
1. **Both direct named locals (<41, `object`!=0):** decl-order fixable. Sweep
   declaration order (reverse-decl controls the pair).
2. **A copy/derived named web (41-50):** number set by creation block/expr
   position, not decl. Only movable by relocating the producing expression
   (stream-affecting) — usually terminal for stream-neutral source.
3. **A TEMP / SR-IV (52+) vs anything lower:** the temp ALWAYS outranks and
   claims first. The ONLY reachable fix is JAMMING the lower web (raise its
   pre-RA degree >=29 so it enters the jam-core ahead of the temp). Jamming is
   a register-PRESSURE property driven by the inline/interference structure, not
   by any C-level reordering. This is the vi1201v1 "inline-boundary" class and is
   terminal for stream-neutral source unless the exact pressure-raising inline
   restructuring is found.
   **SUPERSEDED (2026-08-13): this class is NOT terminal.** See the
   grCastle_801CF868 case below: moving the variable across the inline
   boundary (a wrapper-scope local pops after the entire temp pool) and
   naming the walker as a last-declared local both renumber the web without
   touching the stream. Decode object names and clusters before ruling
   anything terminal.

### Why the melee close-residuals are stuck
mnDataDel: walker (TEMP vr81) vs gobj (copy vr41) -> case 3, jamming needed,
unreachable. ftCo_800A8940: result/spill-ptr/flag are copies vr35-51 -> case 2.
un_8031D9F8: walker temp vs pos/counts copies -> case 3. This is why decl-order
sweeps are inert across the board: the residual swaps virtually never involve two
DIRECT named locals (<41). The gettable class (case 1) is rare because the easy
direct-local swaps are already matched.

## Case: grPura_802125F0 (melee grpura) — SOLVED via replay-search, 2026-08-06

First function matched end-to-end by this pipeline. Residual: one web (the
second-half jobj value) colored r29 (destructive reuse of the dying base) vs
retail r27; 441-instruction stream otherwise identical.

Validated claim rule (reproduced the capture exactly): callee-saved cluster =
webs needing callee-saved registers, selected in descending-vreg order; each
web takes the LOWEST-numbered already-claimed register free w.r.t. colored
neighbors, else claims the next fresh register descending r31→r14.

Replay-search over all cluster orders found the requirement: the misplaced
webs must color AFTER the named webs that claim r28/r27. Since copy-region
(re-def) webs sort above all named webs, the ONLY realizable fix is making
them named-region webs: fresh locals declared last (reverse-decl gives them
the lowest named vregs).

Second blocker and its lever: a def of a VIRGIN named local from a
repeated-GVN-class memory expression lowers as `temp = load; cmp temp;
named = copy temp`, and the copy web-aliases (node flags=4,
physical_register = alias target) only into RENAMED re-def webs — never into
virgin named webs — leaving `lwz r0; cmplwi r0; mr rCS,r0` (or with a
chained-opaque def, a fused `mr. rCS,r0`). The fix: embed the assignment in
the first consuming call argument — `use(x = expr)` — which lowers the load
directly into x's web. Copy-prop defeats alias-based class splits (gp2 = gp
forms) before GVN, so base-aliasing is not a substitute.

Region facts confirmed by this case: the named region is elastic (grows past
40 with more locals; copies start above it); named slots are consumed even by
locals whose webs get renamed to the fresh region (gp=61, gobj=81 here while
their slots 34/35 remained as dead nodes); a user-statement-level null test
after a def (HSD_ASSERT) is associated with the named→fresh renames observed
(gp, gobj), while inline-expansion-internal asserts are not (jobj-A, child
stayed named).

## grOldPupupu_80210D10 (melee groldpupupu) — SOLVED via replay-search, 2026-08-06

Baseline: identical 255-insn stream, 4-web callee-saved cycle (gp/anchor/
magic4330/walker vs retail r31/r29/r30/r27). Two capture+replay rounds, each
yielding exactly ONE winning select order, drove two new general levers:

1. **Param-web carrier.** The parameter web (vreg 32, param arena) is
   special-cased FIRST in the simplify order, ahead of every other band.
   Re-defining a dead-after-first-use param with a derived pointer
   (`gobj = (T*) GET_GROUND(gobj); gp = (T2*) gobj;` as two statements) puts
   the load in the first fresh claim (r31) with zero stream cost. A named
   local can never reach this: decl-with-initializer rank caps inside the
   named/copy super-band and cannot cross obj0 temps.
2. **Static-inline top-pool helper.** Inline-body webs color above the
   0x408b frontend/optimizer arena. Moving a load+assert into a TU-local
   `static inline` helper lifted the loaded pointer's web above a LICM'd
   loop-invariant address walker, fixing the final r28/r27 pair. A statement-
   level `(void) x;` between def and assert is required to keep the load
   landing directly in the carrier (else GVN lowers temp+copy `mr`).

Refined band map (vreg regions, one coloring round): block-scope user webs
(colored from leftovers, excluded from the priority cluster) < function-named
(reverse-decl, elastic) < copy-region renames (creation position) < 0x408b
frontend/optimizer objects < obj0 temps (forward def-block order) < param
homes (claim first). Confirmed by three captures (op1/op2/op3) with the
replayer reproducing every color exactly.

Negative results: decl-init rank never crosses the temp band (a sibling
function's decl-init r31 was conflict-free luck — no callee-saved temps);
GVN-eliminated re-defs coalesce into the SAME named number; parenthesized
calls do not block MWCC inlining; `#pragma dont_inline` is a span toggle that
also disables the wrapped body's internal inlining.

## grHomeRun_8021CB20 solved — simplify law + permanent-degree lever (2026-08-07)

`tools/replay/simplify_replay.py` now replays `Coloring_SimplifyGraph` and
color selection exactly (K=29; validated 145/145 pop rows and colors on
grhomerun and 93/93 on ftCh_Wait1_0_Anim). Key law: web nodes absent from
`simplify_order` are coalesced-to-physical blockers; together with precolored
nodes they are PERMANENT (never simplify, never decrement, block their color).
A web whose permanent degree keeps its dynamic degree >= 29 survives into a
later removal pass and pops earlier; the longest survivor claims r31.

Lever: one staged load-backed call-arg local (`HSD_GObj* text_gobj =
(HSD_GObj*) gp->u.unk.xD4;` passed as the argument) coalesces into the
argument register and adds +1 permanent degree to every web live in its
window, flipping the whole pop head without changing the instruction stream.
Constant-valued staging folds away (no web); staging an argument that already
naturally webs+coalesces (function-pointer lis/addi args) can instead break
its coalescing and emit an extra `mr` — check per site. The hypothesis mode
(`simplify_replay.py CAP IDX 32:+1`) answers "which web needs how much extra
permanent degree" before hunting the source shape.

## Case: melee vi0401 `un_8031D288_OnEnter` — SOLVED, one-field-carrier stratum promotion (2026-08-07)

Full write-up and tools in `(case study withheld)`. Summary: 49-row pure
GPR permutation. Exact replay validated (0 mismatches), DFS witness proved
the target colors order-reachable on the unchanged graph, and a strata-
constrained renumbering search decomposed the fix into three source
levers: (1) loop-2 variable splits move second-region webs into decl
slots; (2) two decl-order swaps; (3) `struct { int i; } idx;` — a
one-field carrier promotes the loop counter's web into the
aggregate-promotion stratum ABOVE the strength-reduction IVs, something
no decl/scope/opacity/chain spelling can do (9! + variants all measured
dead). The carrier stayed fully registered (no memory home, no frame
delta) and as a bonus restored the const-zero u64-store fusion, deleting
the original `li` INSERT without cast hacks. Key rule restated: pops take
the MINIMUM-numbered claimed free register; fresh claims are a strict
r31-descending prefix. Also: objdiff per-symbol scores miss emission-order
errors — always run the section-byte/reloc comparator before closing a TU.

## gm_801BFCFC (Melee GALE01): source-birth-rank vs compiler-temp ordering

`tools/source_rank_solver.py` (reversible source-shape query) was built for this
callee-saved permutation. Findings, all from one capture:

- The 20-row diff is a pure register permutation; the K=29 replay proves the
  target coloring is reachable by *some* virtual-register renumbering (pure
  birth-rank, no graph change).
- Classifying webs by source origin: 12 frontend OBJECTS (named locals) occupy
  vregs 33..45; the first compiler TEMP (the `&gm_8049E558` base) is vr46. Two
  OBJECT slots (37,44) are DEAD — they are the frontend objects of
  `u32* temp_r29`/`temp_r29_2`, whose runtime values live in later call-result
  temps (57,75). The held-pointer declaration is *required* to reproduce the
  target's `bl D970; mr rX,r3; bl lbTime; stw r3,0(rX)` order, so those dead
  slots cannot be inlined away without changing the instruction stream.
- `creation_order_reachable()`: the target IS reachable when the compiler-temp
  block (base < const1 < call-results, fixed by code order) may sit at any
  offset/gap relative to the object block. But the source can only shift the
  block down by whole eliminated objects (fixed internal gaps), and this
  algorithm's 4 loops × (walker+counter) + persistent vars produce a hard floor
  of 12 object webs (variable reuse across loops makes reset webs, so sharing is
  object-count-neutral). Base is pinned at 46; the target needs it ~4 lower.
- Coalescing the base into a loop-walker (share the loop1/loop3 array-A walker)
  makes the base a low OBJECT but the result is creation-order-UNreachable — the
  original (base as a separate temp) is strictly closer to matchable.

Conclusion: no source-level declaration order or expression placement realizes
the target for this exact algorithm; a fewer-object-web implementation producing
the identical instruction stream would be required. The tool converts this from
a manual multi-day sweep into a single reversible query.

The first case script used randomized rank search. The integrated tool now
shares `tools/coloring_model.py`, exactly enumerates bounded declaration spaces,
and distinguishes proof from search failure. A sampled witness remains
constructive, but a sampled miss is reported as `not_found`; only a complete
enumeration may report this constrained source-rank model `unreachable`.

## Case: grCastle_801CF868 (melee grcastle) — the "case 3 terminal" verdict OVERTURNED; vreg-cluster model (2026-08-13)

Matched 100% and TU linked (DOL byte-identical) after two prior sessions had
declared the residual "terminal from C". Two things unlocked it: decoding
OBJECT NAMES from the provenance, and recognizing that vreg numbers are set by
a CLUSTER structure that the inline boundary and declaration order both steer.

### Object-name decoding (do this FIRST for any coloring residual)

`provenance.json` → `virtual_register_creations[*].object_before
.opaque_value_0a_data` embeds the object's name: hex-decode and take the first
printable run after byte 10 (`re.search(rb'[ -~]{2,}', raw[10:])`). Named
locals show their C names (`gobj`, `gp`); frontend/optimizer temps show
`@999`, `@998`, ... with @-names DESCENDING in creation order, so the
vreg↔@-name map is monotone: **vreg order = object creation order**. Gaps in
the @-sequence are created-then-deleted temps. Generate provenance from any
capture with `tools/allocator_provenance.py CAP/allocator-NNNN.json --coloring
CAP/coloring-NNNN-gpr-01-before.json --creations
CAP/pcode-creations-NNNN-initial.json --output CAP/provenance.json`, or print
the table directly with `tools/vreg_map.py CAP NNNN`.

### The cluster model (refines the 2026-08-06 numbering-region law)

For a wrapper function whose body is one auto-inlined call (and by extension
any function with inline expansions):

- **Cluster 0 (vr32..)**: the OUTERMOST function's params and its own named
  locals, in declaration order. Inline-body locals never land here.
- **@-pool, cluster 1**: multi-use LOAD CSE temps for the whole post-expansion
  function, created in REVERSE source order (the last multi-use load is @999).
  A load with a single consumer instead lowers directly into its named web.
- **@-pool, cluster 2**: value temps — call-result copies, ternary merges,
  loop preheader IV copies — also reverse source order.
- **@-pool, cluster 3**: the inline expansion's params + named locals in
  DECLARATION order.
- **vr60+**: lowering scratches in instruction order.

Select order (validated 77-109/109 across four grcastle captures) = jam-core
survivors first, then DESCENDING vreg; claim = highest unused callee-saved;
share = lowest set bit of claimed & ~neighbor-colors. Consequence: a web that
pops after two dead claimers inherits the LOWER of their registers.

Frontend value-merge (named var absorbed into its load-CSE temp, group root =
MIN vreg) happens only WITHIN the pool. A cluster-0 named var cannot merge
with a pool temp: flattening the inline broke `base`'s merge and emitted a
real `mr` (extra instruction). Single-def named vars copy-prop away across the
boundary fine; multi-def ones cannot.

### The reachable fixes for "case 3" (temp/SR-IV outranks a named web)

The 2026-08-06 diagnostic called this class terminal ("only fix is jamming").
grcastle proves THREE stream-neutral levers exist:

1. **Move the variable across the inline boundary.** A wrapper-scope named
   local (cluster 0) pops after the ENTIRE pool, so it colors last and shares
   the lowest freed callee-saved register. grcastle's weight pointer `s32* wp`
   declared in the wrapper (definition kept textually inside the branch, in a
   split phase structure) colored r29; every inline-scope spelling colored
   r30. Constraint: the defining `addi` must be written where the target
   emits it, which may force splitting one inline into two void phase inlines
   (returning-bool phase inlines do NOT thread — the flag materializes as
   li/cmpwi).
2. **Name the walker.** An explicit walking pointer declared LAST
   (`for (p = wp, slot = 0; slot < 3; slot++, p++) rand -= *p;`) replaces the
   frontend's cluster-2 preheader copy with a cluster-3 named web, moving its
   pop position from "after everything" to its declaration slot. This fixed
   the whole volatile trio (p=r3, rand=r4, slot=r5) in one move.
3. **Declaration order within the cluster** (the old case-1 lever) then
   fine-tunes pop order among the remaining locals.

### Ancillary laws confirmed byte-level

- An s32 argument to an inline bills a 4-byte caller-side home placed BETWEEN
  the caller-scope and inline-scope aggregate bands. Removing the argument
  (compute inside the inline) removes the slot. Pointer args did not bill.
- Straight-line accesses through a pointer param holding a KNOWN stack
  address fold to r1+disp (byte-identical to direct member access); in-LOOP
  accesses through the same pointer keep the register walker. This extends
  the cast-pointer fold rule to inline params and lets a phase inline read
  the caller's stack aggregate with zero cost.
- .rodata order: anonymous function-local aggregate initializers emit at
  their function's position; file-scope `static const` defs emit at their
  definition position. The original TU defined its statics BETWEEN functions
  (an early function's `Vec3 {1,0,0}` literal pools first); moving all nine
  static defs below that function fixed the DOL.

### Meta-lessons

- A replay that says "target graph unreachable" while treating vreg numbers
  as fixed is answering the wrong question: the numbering itself is
  source-controlled through the clusters. Decode names and classify clusters
  BEFORE ruling anything terminal.
- Both prior "terminal" proofs were also contaminated by a harness bug
  (absolute-path ninja silently not rebuilding, so ~70 "variants" re-scored a
  stale object). Any sweep infrastructure must delete the output object,
  invoke ninja with a repo-relative path, and fail loudly when the object is
  missing afterward.
