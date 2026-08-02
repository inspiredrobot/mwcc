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
emitted function. A final `ninji` argument selects the verified Melee
GC/1.2.5n identity; the default is stock GC/1.2.5. The address set used here is
confirmed unchanged between those two binaries, but their hashes remain
distinct in every capture.

Auto-capture also traces the two PCode construction wrappers at `0x004a25d0`
and `0x004a2620` through the common builder return at `0x004a2b6d`. Every event
records the allocated instruction, creation epoch, immediate x86 callsite, and
the opaque current-CodeGen-item pointer and 18-byte header from `0x00587130`.
The raw item identity deliberately precedes a semantic AST claim: it preserves
the frontend-to-backend join now, while the item layout is still being
recovered.

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
