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

For register-pressure questions, do not stop at compiler-object allocation.
`mwcc-auto-capture` records object-backed register allocators and every verified
direct counter increment from `config/*/virtual_register_sites.json`.
`tools/explain_register.py` reports these as `virtual_register_origins`. Regenerate
the catalogs with `tools/virtual_register_sites.py`; never add breakpoint
addresses from an unverified disassembly by hand.

CursorThink's objectless `fpr:265` is the canonical direct-temporary case. It is
allocated exactly once at `0x004a05b7` in `Operands_ForceFPR` while a kind-9
memory operand is converted to an FPR. The operand's type size selects LFS or
LFD; this instance selects LFD. The extra live web is therefore formed during
initial PCode lowering, before O4 and coloring. Revisit the source/FE lowering
shape, not register-selection order, when this origin differs between builds.

Use `tools/rank_register_origins.py provenance.json` to find which lowering
sites own the most live webs. Pass `--compare other.json` when testing two source
shapes; the largest live-count deltas identify the frontend/operand operation to
investigate before another spelling sweep.
