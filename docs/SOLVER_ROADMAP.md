# Solver and source-provenance roadmap

The compiler model should eventually answer matching questions in either
direction. It must explain how a source construct produced a PCode value and a
physical register, but it should also accept a target instruction or allocator
decision and enumerate the source-level conditions that could have produced
it.

This is a data-model requirement before it is a solver choice. Recovered
passes should emit durable identities and relations for:

1. source files, tokens, macros, and concrete-syntax-tree (CST) nodes;
2. abstract-syntax-tree (AST) nodes, types, symbols, scopes, and source spans;
3. optimizer expressions and the pass that creates, rewrites, or removes each
   one;
4. PCode instructions, operands, compiler objects, and virtual registers;
5. live ranges, interference edges, coalescing roots, spill decisions, and
   physical colors;
6. frame regions, stack slots, selected instructions, peephole rewrites, and
   scheduling order.

Each relation should cite the exact GC/1.2.5 routine and state transition that
supports it. Unknown links remain explicit; a guessed source-object mapping is
worse than a visible gap.

## Staged deliverables

- Backend provenance v1: map decoded PCode operands through virtual-register
  creation, graph construction, coloring, and final operand rewriting.
- Frontend identity v1: recover enough AST, CST, type, symbol, and source-span
  layout to name the source object associated with a backend object.
- End-to-end trace v1: follow at least one Melee expression from tokens through
  emitted PowerPC instructions, retaining pass-by-pass lineage.
- Reversible query prototype: express common matching questions over exported
  facts and compare candidate engines on the allocator casebook.

`tools/allocator_provenance.py` is the first backend-provenance implementation.
Its flat `mwcc-allocator-provenance-v1` facts use stable structural IDs rather
than compiler addresses. `created_by` maps ordinary surviving PCode to its
construction event, lowering epoch, and immediate x86 callsite, including when
its compiler-object pointer is zero. Optimizer copies instead use `pcode_clones`
and `derived_from`, preserving their exact parent instruction and optimizer
callsite. The capture also retains the current CodeGen-item pointer and
recovered 26-byte header. Item kinds 4 through 15 now decode their `+0x0a`
expression, and expression kind `0x38` joins directly to a `CompilerObject`.
Recovering the remaining expression kinds plus CST, symbol, type, and
source-span links remains the next provenance layer.

The CursorThink `fpr:265` validation demonstrates why that bridge matters: an
objectless allocator node can now be traced to two initial-lowering emissions
from one raw-kind-7 CodeGen item and distinguished from optimizer-created
PCode. The next solver-facing relation should decode `cgN` item fields into the
recovered frontend node/type graph without replacing unknown offsets with
guessed semantic names.

`COpt_005248c0` now supplies a concrete rule for that bridge. Given one PCode
opcode and one `CompilerObject`, it decides whether the object's kind,
addressability, register-info flag, wrapped type kind, size, subtype, and two
type-specific fields create an implicit def/use entry. Frontend identity v1
must therefore preserve at least those exact fields and wrapper edges in its
facts. The next code-motion slices, `COpt_005246d0` and `COpt_005240b0`, can
then record not just a def/use index but the explicit operand or
opcode/object/type rule that created it.

The same validation now has complete pre-coloring instruction provenance. All
20 instructions which bypassed normal constructors were copies made by
`PCode_CloneInstruction`; each maps to one of five live parent instructions.
Four repetitions of the LBZ/CMPLI/BT/ADDI/ADDI body come from callsites
`0x0052aa93` and `0x0052ab71` inside helper `0x0052a200`, reached from the
confirmed loop-transformation pass. The remaining challenge is therefore no
longer unknown scheduler behavior: it is concrete loop-unrolling lineage which
a future pass replay can reproduce.

## Solver experiments

Prolog or Datalog is attractive because compiler relationships are naturally
relational and many useful questions are reversible. It is worth prototyping,
especially for provenance, aliasing, and rule explanation. Register allocation
and source reconstruction also contain weighted choices, bit constraints, and
large search spaces, so SMT, constraint programming, or a specialized search
engine may be more efficient for those pieces.

The first prototype should therefore keep the fact schema independent of the
engine. A solver succeeds only if it predicts focused source changes on held-out
Melee functions and explains rejected candidates using recovered compiler
rules. Replaying assembly from unconstrained guesses is not sufficient.

Two constrained reversible queries now establish the initial engine boundary.
`tools/inverse_coloring.py` inverts physical colors over a fixed captured graph
and reports both required select-order reversals and anonymous pressure-window
lower bounds. `tools/source_rank_solver.py` searches one configurable
object-order band, isolated unused object-slot removal, and the fixed-order
compiler-temporary band. V32 is fixed by default and additional
provenance-known strata can be pinned explicitly. Both tools use
`tools/coloring_model.py`; exact searches and sampled searches carry different
conclusion metadata so a bounded miss cannot become a false unreachability
claim.

The next shared fact layer must replace anonymous pressure windows and coarse
object/temporary classes with source-owned live ranges. It needs scope depth,
shadow-object grants, inline ownership, aggregate promotion, scalar expansion,
coalescing-window eligibility, and first/last PCode use. That layer should feed
both the concrete-edge inverse query and source-edit delta prediction rather
than creating another case-specific replay.
