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

Import or update the executable in the local Ghidra project with:

```sh
ninja ghidra
```

The Melee `GC/1.2.5n` executable is a derived Ninji patch, not the historical
compiler release. It remains available as a secondary configuration so the
patch can eventually be represented in recovered source.

`objdiff.json` is generated for the relocatable target/base objects added as
the PE is split into translation units. The exact host compiler that produced
the executable has not yet been identified, so early source files are
structural decompilation rather than claims of byte matching. Establishing that
host toolchain is Phase 1; see
[docs/HOST_TOOLCHAIN.md](docs/HOST_TOOLCHAIN.md).

See [docs/SCOPE.md](docs/SCOPE.md) for priorities and
[docs/PROVENANCE.md](docs/PROVENANCE.md) for licensing and source provenance.
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
