# Project rules

- Treat `orig/`, compiler binaries, DLLs, and generated Ghidra databases as
  local-only proprietary inputs. Never commit them.
- Verify the configured SHA-256 before drawing conclusions from an executable.
- Record whether every name or structure is proven by this binary, inferred
  from its strings/control flow, or borrowed as a hypothesis from an external
  reference.
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
- Preserve the order and reason for compiler passes as first-class findings.
- Keep commits focused and run `ninja check` plus relevant object diffs before
  committing.
