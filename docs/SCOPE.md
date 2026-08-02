# Scope and milestones

The practical success target is defined in `docs/DECOMP_ASSISTANCE.md`: use the
recovered compiler to make and verify source-level predictions on difficult
Melee matches, rather than relying on compiler folklore or local score changes.

## Phase 1: exact-version analysis

1. Identify the Windows/x86 compiler and linker used to build the stock PE.
   Explicitly test, rather than assume, whether any form of self-hosting is
   possible.
2. Identify the frontend optimizer dispatcher and recover its pass order at
   each optimization level.
3. Identify `CodeGen_Generator` and the backend optimizer dispatcher; recover
   every PCode boundary used by `-O4,p`.
4. Recover the PCode and virtual-register data structures needed for reliable
   dumps.
5. Recover interference graph construction, coalescing, coloring, spilling,
   and saved-register selection.
6. Recover local, argument, spill, and backend-temporary stack allocation and
   its ordering rules.

## Phase 2: matching infrastructure

1. Establish translation-unit boundaries from embedded source filenames,
   control-flow ownership, and data references.
2. Produce relocatable target objects with reconstructed symbols and
   relocations for objdiff.
3. Compile source translation units and report code/data progress.
4. Re-link a byte-identical executable once enough PE/linker metadata is known.

The project deliberately starts with the small vertical slice most useful to
Melee matching. A full compiler decompilation can grow outward from that slice.

## Phase 3: source provenance and solver interfaces

1. Recover token, concrete-syntax-tree (CST), abstract-syntax-tree (AST), type,
   symbol, and source-location layouts needed to identify the source construct
   behind an optimizer or allocator object.
2. Preserve provenance across CST to AST, AST to IR/PCode, virtual-register,
   allocation, frame-layout, and emitted-instruction transitions.
3. Export those transitions as stable relational facts suitable for reversible
   queries and constrained search.
4. Evaluate Datalog/Prolog, SMT, constraint-programming, and specialized search
   engines against real Melee cases; choose based on explanatory power and
   tractability rather than committing to one solver family in advance.

See `docs/SOLVER_ROADMAP.md` for the intended query model and staged
deliverables.
