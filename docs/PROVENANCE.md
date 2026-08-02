# Provenance and licensing

No project-wide license has been selected yet.

## Local proprietary inputs

- Stock `mwcceppc.exe` GC/1.2.5 is the primary local analysis input and is
  excluded by `.gitignore`. Expected SHA-256:
  `0443b5c02b1aa7b575b61e0e24c4d5ad6bed8fd54cc42de5a2204a5216001914`.
- `mwcceppc.exe` GC/1.2.5n is a derived, patched local analysis input and is
  excluded by
  `.gitignore`. Expected SHA-256:
  `ccf4b465cec73b5aae9c5c5543dcf8cda8a62aba246f89e2e0b200d742f2e55c`.
- Any associated Metrowerks DLLs or compiler packages are also excluded.

## Tools and references

- Ghidra 12.0 is used as an external tool under Apache-2.0. No Ghidra source is
  copied into this repository.
- objdiff is used as an external tool under MIT OR Apache-2.0.
- decomp-toolkit is MIT OR Apache-2.0. It is a useful workflow reference, but
  its DOL/REL splitting does not directly handle this PE target.
- retrowin32 commit `11dbea5a68af21121511a6577a2d4a2f917da6dc`
  was audited and built separately under Apache-2.0. It is not vendored here.
- `mwcc-debugger` commit
  `bad9cea2423bed957188c930086f9dabe669d30c` has no published license. A
  locally patched audit copy may be used for experiments, but none of its code
  may be copied here without permission. The allocator snapshot reader in this
  repository was written independently from offsets established in the
  verified GC/1.2.5 executable; the external tool only corroborates PCode
  snapshots as a useful debugging interface.
- Ninji/Ash Wolf's MWCC decomp at `https://git.wuffs.org/MWCC/` is a valuable
  read-only naming and structural reference. No license was established, so no
  source or comments are copied from it.
- RootCubed's `mwcc-inspector` commit
  `e498289031785738a51af779a7ccfa50c6a57a12` is an MIT-licensed debugger
  reference. Its GC/1.2.5 optimizer breakpoint independently corroborates the
  frontend dispatcher address. No source has yet been copied or adapted.
- The build/configuration layout follows conventions used by the
  doldecomp/melee project. `.clang-format` is adapted from Melee's formatting
  configuration; project-specific include categories were omitted.
- Carnegie Mellon Graphics Lab's historical download archive is used only as a
  source of external comparison objects; no package content is committed or
  executed. The inspected archives and SHA-256 hashes are:
  - `CW5_Win32_CMUgraphics15_source.zip`:
    `5208d3e04c628cece55623545b1cd5a40ef925a2e9e37c8bc668e2a951d670aa`
  - `CW5_Win32_CMUgraphics15_binary_console.zip`:
    `e70f4cd2ba295a2e5c7bec3a12bd7ab439cb4f5cf5273aa9a1cd2d0cba828017`
  - `CMG2_Win32_CW6_source_2_1_5.zip`:
    `fa315a60f3863f64831111ec3bff276408bc2c17bab26ff223fa0467c8522576`
  - `CMG2_Win32_CW6_starter_2_1_5.zip`:
    `96e89a420d91213d43b2c775f0083d9f10b01a8e30e2e9fdbcb296275910ff90`
  Acquisition details are intentionally retained only on the local
  workstation.

For every imported or adapted file, add its source URL, revision, license, and
material modifications here.
