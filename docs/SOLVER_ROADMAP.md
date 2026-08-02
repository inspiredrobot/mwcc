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
callsite. The capture also retains an opaque current-CodeGen-item pointer and
header as the first frontend/backend bridge. Decoding that item through AST,
CST, symbol, type, and source-span layouts remains the next provenance layer.

The CursorThink `fpr:265` validation demonstrates why that bridge matters: an
objectless allocator node can now be traced to two initial-lowering emissions
from one raw-kind-7 CodeGen item and distinguished from optimizer-created
PCode. The next solver-facing relation should decode `cgN` item fields into the
recovered frontend node/type graph without replacing unknown offsets with
guessed semantic names.

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
