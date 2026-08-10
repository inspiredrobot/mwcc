# Agent decompilation workflow

## Establish the target

Run `python3 configure.py` followed by `ninja check`. This verifies the stock
SHA-256, PE metadata, core function addresses, source placeholders, and exact
trace strings. It also compiles every recovered translation unit with strict
C90 host syntax checks; this is a structural check, not a byte-match claim.
Never begin from the patched `GC_1_2_5n` image by accident.

Before committing C or header changes, run:

```sh
python3 tools/check_format.py --fix
ninja check
```

`ninja check` also runs host-side behavioral tests for reconstructed models.
These tests establish functional invariants; they do not claim that the host
compiler reproduces the target x86 instruction stream.

The compiler binary is untrusted data. Static analysis does not execute it. Do
not invoke it on the host or through host Wine/Wibo; follow the sandbox policy
in `AGENTS.md` if dynamic behavior ever becomes essential.

## Refresh static analysis

```sh
ninja ghidra
ninja subsystem-inventory optimizer-analysis
```

Generated reports live under `build/GC_1_2_5/` and are intentionally ignored.
The durable conclusions belong in `docs/`, the subsystem manifest, and
`config/GC_1_2_5/symbols.csv`.

Export a focused function set with:

```sh
python3 tools/ghidra_project.py export-functions \
  --config config/GC_1_2_5/config.json \
  --output build/GC_1_2_5/focus.md \
  0x004351c0 0x004c4430
```

## Work from boundaries inward

1. Start at an exact source filename, trace string, or known caller boundary.
2. Record raw address, callers, callees, strings, and global offsets before
   choosing a semantic name.
3. Reconstruct control flow without prematurely inventing data structures.
4. Build offset-use tables and promote fields only when corroborated.
5. Keep repeated optimizer passes and class-specific allocator paths explicit.
6. Add every durable conclusion to the manifest/docs and run `ninja check`.

## Matching strategy

CodeWarrior Pro 5 Win32/x86 2.3 is validated as the host family and can now be
used as a matching fitness function for focused leaves. The exact minor build
is still being fingerprinted, so a mismatch in call-bearing code must be
classified against source shape, headers, and compiler flags before changing a
clean structural decompilation. Test candidate Release configurations only in
the offline sandbox.
Melee's PowerPC `-O4,p` flags describe the compiler's output behavior and must
not be confused with the flags that built this Win32 compiler executable.
Functions may be marked functionally equivalent before binary matching is
available. Their `match_percent` must remain `null`, with a reason, until an
actual candidate host compiler produces a comparable object.

## Explain register-pressure changes

Use `mwcc-auto-capture` inside the hardened offline debugger described in
`docs/CAPTURE_EXPERIMENTS.md`. For a focused Melee function, the capture
directory contains four PCode stages, the allocator input, coloring graphs,
and a creation trace for each stage. Join the allocator input to the
allocator-phase trace, not an earlier partial trace:

```text
mwcc-auto-capture /capture grInishie1_801FB3F0 ninji
```

An exact symbol selector avoids guessing the emitted-function index. The
capture reads the cached name record used by target routine `0x004c2560` and
records `function_identity` in every generated artifact. Numeric indices
remain supported. Kind-5 function objects have no non-invasive cached record
and must still be selected by index.

```sh
python3 tools/allocator_provenance.py \
  capture/allocator-0015.json \
  --coloring capture/coloring-0015-gpr-01-before.json \
  --coloring capture/coloring-0015-gpr-01-after.json \
  --coloring capture/coloring-0015-fpr-01-before.json \
  --coloring capture/coloring-0015-fpr-01-after.json \
  --creations capture/pcode-creations-0015-allocator.json \
  --output capture/provenance.json
```

The exporter selects `config/GC_1_2_5/virtual_register_sites.json` or the
Ninji catalog from the capture's compiler SHA-256. This lets an old raw capture
gain newly recovered function and operation names without rerunning the
compiler. Pass `--register-sites PATH` only when deliberately using another
verified catalog.

Explain one web when the final diff already identifies it:

```sh
python3 tools/explain_register.py capture/provenance.json gpr:399
python3 tools/explain_register.py capture/provenance.json fpr:265
```

`virtual_register_origins` distinguishes object-backed allocation from a
compiler temporary and gives the exact allocator/lowering site. Definition and
use records then connect that birth to PCode creation, optimizer clone ancestry,
interference, simplify order, and final color.

New captures also expose `virtual_register_counter_intervals`, which assign
each half-open GPR, FPR, and vector number range to the PCode stage that minted
it. Coloring records add the active `coalescing_windows` and the complete
`coalescing_groups`, including direct parent, resolved root, members, and spill
costs. `explain_register.py` includes the matching interval, window, and group
for the requested web. Site-catalog labels apply retroactively to old raw
captures, but parent maps and stage-boundary counters require a fresh capture.
The explanation's `initial_object_strata` field additionally identifies webs
inside the counter range closed by the first object-preallocation walk.

Rank a whole function when the responsible web is not known:

```sh
python3 tools/rank_register_origins.py capture/provenance.json --limit 20
```

Each group reports total allocations, live and dead allocations, the first and
last live virtual-register IDs, and the PCode mnemonics that define the live
webs. It also reports the operation category and its evidence source when the
site catalog has one. A high live count identifies a lowering routine worth
decompiling; a high dead count identifies work removed before allocation.

For a controlled source experiment, capture both variants and compare them:

```sh
python3 tools/rank_register_origins.py baseline/provenance.json \
  --compare candidate/provenance.json --limit 20 \
  --output origin-delta.json
```

All deltas are candidate minus baseline. Start with the largest absolute
`live_delta`; use `allocated_delta` to distinguish a new birth from a lifetime
that merely became dead or live. Only then inspect register IDs and final
colors, since inserting one early web can renumber every later temporary.

When aggregate counts stay equal but the physical coloring changes, align the
semantic webs rather than comparing their numeric virtual-register IDs:

```sh
python3 tools/align_register_webs.py \
  baseline/provenance.json candidate/provenance.json \
  --register-class gpr --output web-alignment.json
```

The report combines PCode position/signatures, creation and clone lineage,
allocator origin and object type, lifetime shape, and graph shape. It preserves
alternative candidates and confidence for ambiguous mappings, then reports
old/new color, simplify position, allocator stratum, and semantic edge changes
for confident mappings.

If aligned target instructions establish the desired colors, invert the
captured selection model before trying source edits:

```sh
python3 tools/inverse_coloring.py capture/coloring-0007-gpr-01-before.json \
  --after capture/coloring-0007-gpr-01-after.json \
  --provenance capture/provenance.json \
  --target 32=31 --target 56=29 --degree-search 6
```

First require a complete baseline replay. An order-only hit proves that the
target colors are reachable on the fixed graph and reports required precedence
reversals. A synthetic degree hit reports both a pressure lower bound and the
minimum anonymous live-range overlap cover. It still does not model those
new webs' own simplify lifetime, color, or source origin. Record that gap in
`docs/requests/` rather than treating anonymous overlap windows as a source
prediction.

If the fixed graph is correct and only source birth rank is in question, use
the constrained source-rank query:

```sh
python3 tools/source_rank_solver.py capture 7 \
  --target 35:31,34:30,46:27 --output source-rank.json
```

The implemented source family permits one object-band permutation, removal of
graph-isolated object slots with no PCode occurrences, and the resulting shift
of the fixed-order compiler-temporary band. V32 is fixed automatically; use
`--fixed-object` for every additional parameter, shadow, inline, aggregate, or
otherwise non-permutable object known from provenance. Exact bounded searches
can prove this configured family reachable or unreachable. Larger searches are
sampled deterministically and report a miss as `not_found`; only a witness is
conclusive in that mode.

Fresh auto-captures also write `stack-frame-NNNN.json`. Join one to allocator
provenance to see each addressed compiler object in allocation order, its type
size/alignment, cursor padding, local-band offset, final SP-relative slot, and
live PCode ownership evidence:

```sh
python3 tools/stack_frame_trace.py capture/stack-frame-0007.json \
  --provenance capture/provenance.json
```

Compare controlled variants with both provenance files when available:

```sh
python3 tools/stack_frame_trace.py baseline/stack-frame-0007.json \
  --provenance baseline/provenance.json \
  --compare candidate/stack-frame-0007.json \
  --compare-provenance candidate/provenance.json \
  --output stack-delta.json
```

Only unique semantic signatures are aligned automatically. Repeated locals
with identical type and PCode-use signatures are reported as an ambiguous
group instead of being paired by allocation order.

The CursorThink validation is the completeness test for this workflow: all 695
live GPR webs and all 273 live FPR webs have exactly one birth origin. Its
largest source is `Operands_ForceGPR`'s load path, while objectless `fpr:265`
comes from `Operands_ForceFPR`'s direct LFD path at `0x004a05b7`.
