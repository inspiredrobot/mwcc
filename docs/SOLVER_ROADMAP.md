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
than compiler addresses. The next relation is `created_by`: a PCode instruction
must point to the exact lowering callsite that emitted it, including when its
compiler-object pointer is zero.

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
