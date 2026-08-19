# Project rules

- Treat `orig/`, compiler binaries, DLLs, and generated Ghidra databases as
  local-only proprietary inputs. Never commit them.
- Treat every downloaded binary as untrusted. Parse object files and libraries
  as data only. Never execute a third-party EXE or load a third-party DLL on the
  host. If execution becomes necessary, use a disposable sandbox with no
  network, read-only inputs, and a dedicated writable scratch directory.
- Do not run candidate tools through host Wine or Wibo. A sandboxed run must
  also use a read-only root filesystem, drop Linux capabilities, enable
  `no-new-privileges`, and impose process and resource limits. Preserve the
  input hash, exact command line, and sandbox configuration with the result.
- Verify the configured SHA-256 before drawing conclusions from an executable.
- Record whether every name or structure is proven by this binary, inferred
  from its strings/control flow, or borrowed as a hypothesis from an external
  reference.
- This project's own contents are CC0 1.0 (public domain); anyone may reuse
  them for anything, with no attribution or permission needed. Keep new files
  under that dedication and do not add per-file copyright headers.
- The compiler studied here is proprietary and is never redistributed by this
  project. This is noncommercial research intended as fair use, and it will be
  taken down at a rights holder's request; never commit anything that would
  make the repository a substitute for the product.
- Do not copy third-party code unless its license or explicit permission allows
  it. Record copied files and modifications in `docs/PROVENANCE.md`.
- `mwcc-debugger` and Ninji's MWCC decomp are reference-only until their
  licenses are clarified. Facts learned from them must be independently
  checked against stock GC/1.2.5.
- Treat GC/1.2.5 as the primary historical target. GC/1.2.5n is a derived
  patch configuration, not a replacement for the stock artifact.
- Do not assume the compiler was self-hosted. Keep host compiler and linker
  identification evidence in `docs/HOST_TOOLCHAIN.md` and require output
  comparison before calling a candidate confirmed.
- Prefer typed, structured C over assembly-shaped output. Do not encode a
  guessed structure merely to improve a superficial diff.
- Prefer typed static helper functions over statement-like macros when shared
  behavior has meaningful inputs or state. Keep macros for declarations and
  compile-time concerns, not reconstructed control flow.
- Preserve the order and reason for compiler passes as first-class findings.
- Before core backend work, read `docs/CORE_SUBSYSTEMS.md`,
  `docs/DATA_MODEL.md`, `docs/DECOMP_WORKFLOW.md`, and
  `docs/DECOMP_ASSISTANCE.md`. Update the subsystem manifest when an
  address-backed role or name is confirmed or rejected.
- Keep commits focused and run `ninja check` plus relevant object diffs before
  committing. Run `python3 tools/check_format.py --fix` before committing C or
  header changes; `ninja check` enforces the result.
