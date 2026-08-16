# Matching-tool request status

This file groups recurring requests from Melee matching sessions by shared
compiler capability. Case-specific evidence remains in `docs/requests/` and
`docs/ALLOCATOR_CASEBOOK.md`; implementation status belongs here so the same
gap is not rediscovered under several function names.

| Capability | Status | Reusable implementation | Remaining boundary |
| --- | --- | --- | --- |
| Exact simplify and color replay | Implemented for captured GPR cases | `tools/coloring_model.py`; compatibility CLI in `tools/replay/simplify_replay.py` | Capture the available-color count explicitly for general FPR/VR simplify replay |
| Fixed-graph target-color inversion | Implemented | `tools/inverse_coloring.py` | Large prefixes need a constraint solver instead of factorial enumeration |
| Pressure lower bound and overlap windows | Implemented as an abstract bound | `--degree-search` reports minimum degree additions, pressure-web count, canonical overlap windows, and unavoidable pairwise overlaps | Replay concrete new/extended webs with source ownership and lifetime constraints |
| Source birth-rank reachability | Implemented for one configurable object-order band, isolated unused-slot removal, and fixed temporary order | `tools/source_rank_solver.py`; v32 fixed by default and repeatable `--fixed-object` constraints | Infer parameter, scope-depth, shadow-object, inline-parameter, aggregate-promotion, and scalar-expansion strata from provenance |
| Capture selection and labeling by symbol | Implemented and live-validated from exact routine `0x004c2560` | `mwcc-auto-capture DIR NAME [stock\|ninji]`; `function_identity` in every artifact and provenance export | Validate additional cached CMangler kinds; kind 5 has no non-invasive cached name |
| Target assembly to desired web colors | Open | Semantic web alignment provides the candidate half | Add aligned target/candidate operand ingestion with ambiguity-preserving constraints |
| Copy/coalescing inverse query | Open | Captures contain parent maps, groups, windows, and PCode copy operands | Explain a missing/preserved copy as the smallest ownership or window change |
| Loop-carried scalar promotion | Open; highest frontend priority | Code-motion census, object index, type predicate, and first motion pass are reconstructed | Recover the later loop-transform decisions that choose named-object versus optimization-temp ownership |
| Shadow-object grant and first-region ownership | Open | Initial object boundary, creation strata, and coalescing windows are captured | Recover the frontend/optimizer grant pass and predict the selected live region |
| Offline source-edit delta prediction | Open | Origin comparison and source-rank search cover two constrained edit families | Join CST/AST scope edits to object births, optimization rewrites, and PCode deltas |
| Post-register-allocation rewrites | Open; blocks a live case | Nothing yet: captures stop at the allocator input and the coloring snapshots | Snapshot PCode after coloring and spill rewriting, so the pass that merges address-chain instructions becomes observable |

## Interpretation rules

- A replay that matches a captured after-snapshot is an exact statement about
  the modeled allocator path for that capture.
- An inverse-order witness proves target colors are reachable on the fixed
  graph, not that simplify or source lowering can produce the order.
- A pressure overlap report is an edge-count lower bound. Its anonymous live
  ranges do not yet have simplify lifetimes, colors, or source owners.
- A source-rank witness is constructive inside the configured stratum model. An
  exhaustive miss proves only that model unreachable. A bounded sampled miss
  is `not_found` and must not be described as terminal.
- An exhaustive `unreachable` must also never be described as terminal for the
  FUNCTION: it binds one reconstruction's expression structure. Source
  restructuring (the solver's printed `realization_levers`) changes the capture
  and re-opens the search. Melee mplib (upstream PR #(withheld)) falsified a
  "terminal via clean source" conclusion built on such a verdict — the rank
  prediction was correct and the realization step was the failure (casebook:
  "mplib accessor twins").
- Function-name capture is a static read of the same cached name record used by
  GC/1.2.5 routine `0x004c2560`; it does not call code in the untrusted target.

## Case routing

- `grHomeRun_8021CB20`: shadow ownership remains the open layer.
- `it_80289BE8`: loop-carried scalar promotion and target-to-web inference.
- `grInishie1_801FB3F0`: concrete pressure-web/source-lifetime realization,
  then copy/coalescing inversion for the two missing moves.
- `gm_801BFCFC`: source-rank solver validation and future richer stratum
  constraints.
