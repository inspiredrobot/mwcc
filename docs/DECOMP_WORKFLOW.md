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

The host toolchain is still being fingerprinted. Until it is confirmed, a
clean structural decompilation is the objective; byte matching is not yet a
reliable fitness function. When candidate x86 compilers are available, test
their Release configurations and exact project flags in the offline sandbox.
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

Rank a whole function when the responsible web is not known:

```sh
python3 tools/rank_register_origins.py capture/provenance.json --limit 20
```

Each group reports total allocations, live and dead allocations, the first and
last live virtual-register IDs, and the PCode mnemonics that define the live
webs. A high live count identifies a lowering routine worth decompiling; a high
dead count identifies work removed before allocation.

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

The CursorThink validation is the completeness test for this workflow: all 695
live GPR webs and all 273 live FPR webs have exactly one birth origin. Its
largest source is `Operands_ForceGPR`'s load path, while objectless `fpr:265`
comes from `Operands_ForceFPR`'s direct LFD path at `0x004a05b7`.
