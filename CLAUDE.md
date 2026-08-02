Read and follow `AGENTS.md`.

The durable research notes are in `docs/`. Update them when a hypothesis is
confirmed or rejected so future sessions do not repeat the same work.

For backend captures, generate `build/GC_1_2_5/pcode-opcodes.json` with
`ninja pcode-opcodes`; allocator snapshots also include these exact live opcode
descriptors. The longer-term AST/CST provenance and reversible-query plan is in
`docs/SOLVER_ROADMAP.md`.

Use `tools/allocator_provenance.py` before manually correlating allocator and
coloring captures. It emits stable block, instruction, operand, virtual-register,
graph-node, interference-edge, simplify-order, coalescing, and object-binding
facts. An operand object of zero means the backend did not retain source-object
identity; do not infer that the value was source-authored.

For frontend-versus-backend pressure questions, use `mwcc-auto-capture` and
compare `pcode-NNNN-initial.json` with `pcode-NNNN-optimized.json`. Join the
optimized creation trace through `allocator_provenance.py --creations`, then
query the suspect web with `tools/explain_register.py`. This recovers its exact
PCode creation callsite even when the coloring node's object pointer is zero.
Use the optional emitted-function index to timebox large TUs, and pass `ninji`
when running Melee's verified GC/1.2.5n derivative so captures are not
misidentified as stock.

Do not classify a live PCode instruction without a normal creation event as a
scheduler artifact. Compare the optimized, scheduled, and forward-peephole
snapshots first. `mwcc-auto-capture` traces `PCode_CloneInstruction` during O4,
and `allocator_provenance.py` emits exact `derived_from` links for surviving
clones.

For memory/immediate operand questions, inspect
`creation_operands[*].compiler_object` before changing source shape.
`PCodeUtilities_BuildInstructionV` uses the exact object kind/type flags
recorded there for `m`, `M`, and `l`; a zero object and an object-backed kind-4
operand are different lowering cases.
