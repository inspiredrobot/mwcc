# Decompilation-assistance target

The project’s north-star is not merely a recompilable MWCC executable. It is a
version-specific explanation engine for difficult matching problems. Given a
Melee source fragment and its target assembly, the recovered compiler model
should produce testable answers to questions such as:

- which optimizer pass created, removed, or combined an expression;
- why a value was memory-homed instead of propagated;
- why two live ranges did or did not coalesce;
- why a particular physical register won coloring;
- which spill or saved-register rule changed the stack frame;
- which frame region owns a stack slot and why it appears in that order;
- whether an instruction-order difference comes from selection, peephole, or
  scheduling.

## Assistance v1 milestone

Version 1 is complete when the project can analyze a curated set of five Melee
functions previously considered plateaued or “kit-only” and, for each one:

1. classify the important mismatches by optimizer, allocator, frame-layout,
   instruction-selection, or scheduler stage;
2. cite the exact GC/1.2.5 routine and state transition behind each claim;
3. predict at least one focused source experiment before compiling it;
4. record whether the resulting object diff confirms or rejects the model.

At least three of the five cases must yield a confirmed matching improvement.
A rejected prediction is still useful when its inputs and observed result are
recorded; unexplained hill climbing is not.

## Required compiler model

The initial dependency chain is deliberately narrow:

1. frontend and backend optimization pass order;
2. PCode instruction, operand, object, and basic-block layouts;
3. virtual-register creation and explicit physical-register binding;
4. interference construction, coalescing, coloring, and spilling;
5. saved-register accounting and stack-region layout;
6. instruction selection, peephole rewrites, and scheduling decisions needed
   by the case studies.

Recovered source should express shared mechanisms with typed functions and
small data structures. Address-suffixed names remain preferable to confident
but unsupported semantics. Every reconstructed target function records both a
functional status and a binary match percentage; percentages remain
“unmeasured” until a confirmed host compiler can produce comparable objects.

## Deliverables

- a compilable, host-testable functional model of the core routines;
- address-backed layouts and pass rules in machine-readable manifests;
- focused static-analysis exports that can be regenerated from the verified
  executable;
- small probes for isolated optimizer and allocator decisions;
- a Melee casebook containing predictions, experiments, and outcomes.

This makes progress cumulative: a compiler rule learned for one translation
unit becomes a reusable diagnostic for every later matching effort.
