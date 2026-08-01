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
