# MWCC GC/1.2.5 decompilation

This project reconstructs the Windows-hosted Metrowerks PowerPC compiler used
by the Melee build. The immediate goal is a verified account of the compiler's
frontend and backend optimization pipelines, register allocator, and stack
allocator. The longer-term goal is a matching decompilation of
`mwcceppc.exe`.

The primary target is the stock compiler. The original executable is
proprietary and is never committed. Place it at:

```text
orig/GC_1_2_5/mwcceppc.exe
```

The expected SHA-256 is:

```text
0443b5c02b1aa7b575b61e0e24c4d5ad6bed8fd54cc42de5a2204a5216001914
```

Configure and verify the project with:

```sh
python3 configure.py
ninja check
```

Export the compiler's complete PCode mnemonic and operand-format catalog with:

```sh
ninja pcode-opcodes
```

Join an allocator capture to its coloring graph and emit flat provenance facts
with:

```sh
python3 tools/allocator_provenance.py allocator.json \
  --coloring coloring-before.json --coloring coloring-after.json \
  --creations pcode-creations-optimized.json \
  --output provenance.json
```

Use the allocator-phase creation trace when register-birth provenance is
needed. The exporter automatically enriches exact allocation addresses from
the verified catalog matching the capture's compiler SHA-256. To rank the
lowering sites responsible for live virtual registers:

```sh
python3 tools/rank_register_origins.py provenance.json --limit 20
```

To compare two source variants, build one provenance file for each and treat
the first as the baseline:

```sh
python3 tools/rank_register_origins.py baseline.json \
  --compare candidate.json --limit 20
```

`allocated_delta` and `live_delta` are candidate minus baseline. The report
groups by register class, allocation kind, and exact compiler site, then adds
the recovered function/operation name and definition mnemonics. This identifies
the lowering decision that changed pressure before inspecting final physical
register permutations.

Then explain one allocator web or compare the initial and optimized PCode:

```sh
python3 tools/explain_register.py provenance.json fpr:265
python3 tools/compare_pcode_stages.py pcode-initial.json pcode-optimized.json \
  --creations pcode-creations-optimized.json
```

Optimizer clone tracing is included in the same capture. Provenance exports
connect surviving clones to their parent instructions with `derived_from`; the
register explanation reports that ancestry and the optimizer clone callsite.

Inside the offline debugger, `mwcc-auto-capture DIRECTORY 15 ninji` limits an
expensive trace to emitted function 15 and labels it with the verified Melee
GC/1.2.5n identity. Omit the index to capture every function; omit `ninji` for
the stock GC/1.2.5 target.

The checked-in direct-allocation catalogs are derived from the verified PE and
validated by `ninja check`. Regenerate one only from its matching local binary:

```sh
python3 tools/virtual_register_sites.py \
  --config config/GC_1_2_5/config.json \
  --output config/GC_1_2_5/virtual_register_sites.json
```

See [docs/DECOMP_WORKFLOW.md](docs/DECOMP_WORKFLOW.md) for the complete
capture-to-comparison workflow and interpretation rules.

Import or update the executable in the local Ghidra project with:

```sh
ninja ghidra
```

The Melee `GC/1.2.5n` executable is a derived Ninji patch, not the historical
compiler release. It remains available as a secondary configuration so the
patch can eventually be represented in recovered source.

`objdiff.json` is generated for the relocatable target/base objects added as
the PE is split into translation units. CodeWarrior Pro 5 Win32/x86 2.3 is now
validated as the host family: at `-O4,p`, it reproduces four of five focused
target functions (one modulo a normal address relocation), and its runtime
library contributes 693 more exact target bytes. The exact host minor build is
not yet proven, so early subsystem files remain structural decompilation until
their candidate translation units are wired. See
[docs/HOST_TOOLCHAIN.md](docs/HOST_TOOLCHAIN.md).

The first real subsystem pass also makes all three register color-mask helpers
and all three available-register counters instruction-exact, for six checked-in
100% functions.

Measure a sandbox-generated probe object with:

```sh
python3 tools/host_probe_match.py \
  --config config/GC_1_2_5/config.json \
  --object /dedicated/output/codegen.obj
```

See [docs/SCOPE.md](docs/SCOPE.md) for priorities and
[docs/PROVENANCE.md](docs/PROVENANCE.md) for licensing and source provenance.
Auditable offline runtime experiments are recorded in
[docs/CAPTURE_EXPERIMENTS.md](docs/CAPTURE_EXPERIMENTS.md).
The working subsystem map and agent workflow are in
[docs/CORE_SUBSYSTEMS.md](docs/CORE_SUBSYSTEMS.md) and
[docs/DECOMP_WORKFLOW.md](docs/DECOMP_WORKFLOW.md). The concrete allocator
replay target and its first five Melee cases are in
[docs/ALLOCATOR_CASEBOOK.md](docs/ALLOCATOR_CASEBOOK.md).
The staged AST/CST provenance and reversible-query plan is in
[docs/SOLVER_ROADMAP.md](docs/SOLVER_ROADMAP.md).
Two useful external references are RootCubed's MIT-licensed
[mwcc-inspector](https://github.com/RootCubed/mwcc-inspector) and
[mwcc-debugger](https://github.com/cadmic/mwcc-debugger), whose code is not
copied because that repository does not publish a license.

## Binary safety

Compiler executables, DLLs, object files, and libraries are untrusted inputs.
Static inspection is preferred. Never execute a downloaded binary on the host
or load it through host Wine/Wibo. If dynamic behavior is essential, use a
fresh disposable sandbox with networking disabled, a read-only root
filesystem and input mounts, dropped capabilities, `no-new-privileges`, strict
resource limits, and a dedicated writable scratch mount. Record the artifact
hash and complete sandbox invocation so the experiment is auditable.
