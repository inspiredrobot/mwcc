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
