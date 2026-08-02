# Host toolchain identification

The decompilation target is a Win32/x86 PE executable, but the program identifies
itself as the Metrowerks compiler for Embedded PowerPC. Identifying the compiler
that built this compiler is therefore a separate problem from identifying its
PowerPC code generator.

## Findings

### The stock target and the patch are distinct

The primary artifact is the stock `GC/1.2.5/mwcceppc.exe`:

- SHA-256 `0443b5c02b1aa7b575b61e0e24c4d5ad6bed8fd54cc42de5a2204a5216001914`
- PE machine `IMAGE_FILE_MACHINE_I386`
- runtime build `Apr 23 2001 10:58:30`
- product version `2.3.3 build 163`

The executable distributed in Melee's `GC/1.2.5n` directory is a later Ninji
patch with SHA-256
`ccf4b465cec73b5aae9c5c5543dcf8cda8a62aba246f89e2e0b200d742f2e55c`.
The two files have equal size and differ in 53 bytes. The frontend optimizer
dispatcher at `0x0042cd10` is byte-identical across them.

### Literal self-compilation is not possible

The stock executable's own help lists only Embedded PowerPC processors: 401,
403, 5xx, 6xx, 7xx, 8xx, 74xx, 82xx, Gekko, and generic PowerPC. Searches of
the complete option help find no x86 or Win32 target. Compiling
`probes/host/minimal.c` produces a PowerPC object, while the compiler itself is
an i386 PE. Thus this `mwcceppc.exe` cannot directly reproduce its own machine
code.

This does not rule out bootstrapping from shared compiler sources. Metrowerks
had sibling x86/Win32 code-generation tools, and the target's `.exc`, `.CRT`,
`.tls`, and `.bss` sections plus statically linked Metrowerks runtime strings
are consistent with that family. The exact sibling version and linker are not
yet confirmed.

### Contemporary evidence narrows the hypothesis

A February 2001 article by Metrowerks' Richard Atwell says that, except for the
compilers themselves, everything on the CodeWarrior Pro 6 CDs was built with
Version 6.0. This is direct evidence against assuming the released Pro 6
compiler binaries were built with themselves. The leading hypothesis is now a
preceding Win32/x86 MWCC lineage, likely the compiler used to bootstrap the Pro
6 family. This remains a hypothesis until output is compared.

### A CodeWarrior 5.3 object has an exact target-code fingerprint

Carnegie Mellon University still distributes source and prebuilt Win32
libraries explicitly labeled for CodeWarrior Pro 5.3 and CodeWarrior 6. Static
inspection of the Pro 5.3 `CMUgraphics.lib` found this relocation-free body for
libjpeg's `_jzero_far`:

```text
31 c0 57 8b 4c 24 0c 8b 7c 24 08 f3 aa 5f c3
```

The 15 bytes occur exactly once in the stock compiler's `.text`, at
`0x00441db0`. They decode to `xor eax,eax`, save `edi`, load a byte count and
destination from the stack, `rep stosb`, restore `edi`, and return. This is
strong evidence that the target shares optimized x86 code-generation behavior
with the Pro 5.3 toolchain. It is not yet proof of the exact compiler version:
the idiom may be stable across releases, and the original C spelling and build
flags still need to be reproduced.

A scan of 82 Pro 5.3 objects found no other relocation-free exact body of at
least 12 bytes. The sampled CodeWarrior 6 library yielded no exact matches, but
its objects contain frame pointers, `0xcccccccc` stack initialization, and
CodeView debug sections. That sample is plainly a debug configuration and
cannot be used to reject CodeWarrior 6 against the optimized target.

### The CodeWarrior Pro 5 x86 compiler reproduces target code

An original-media preservation of the CodeWarrior Professional Release 5
Windows Tools CD contains the standalone `mwcc.exe`, `mwld.exe`, and Win32
runtime libraries. The disc is not an official CMU download, so its provenance
is recorded separately in `(manifest withheld)`. Static inspection
established the ISO9660 volume `CW_PRO5`, the embedded tools ZIP, and every
component hash before any executable was run.

The hash-verified command-line compiler reports:

```text
Metrowerks C/C++ Compiler for Windows/x86.
Copyright (c)1995-1999 Metrowerks, Inc.
All rights reserved.
Version 2.3
Runtime Built: May 26 1999 17:53:15
```

It was run only through `tools/run_host_candidate.py`, which builds a
non-shell Docker invocation with no network, a read-only root and input mounts,
dropped capabilities, `no-new-privileges`, PID/CPU/memory/time limits, and a
dedicated writable output. The report records the compiler and runner hashes,
container image ID, exact argument vector, output, and exit status.

The important flag distinction is proven by the compiler's own help and by
output comparison. `-O4` enables level-four optimization and intrinsics, while
`-O4,p` also selects speed optimization and scheduling. The latter reproduces
the target's aligned branch destinations and padding. With `-O4,p`, the first
probe pass gives:

| Probe | Result |
| --- | --- |
| `absolute_int` | 17/17 bytes exact |
| `short_predecessor` | 22/22 bytes exact |
| `initialize_four_words` | 34/34 bytes exact |
| `test_bit` | 28/28 comparable instruction bytes exact; four address-relocation bytes excluded |
| `xor_64` | 14/25 bytes exact with the current compound-assignment spelling |

Thus four of five functions are instruction-exact. The whole five-function
set is 89.23% raw byte match and 91.27% over bytes comparable before linking.
The four successful functions are 100% over all comparable bytes. The
remaining `xor_64` is a useful discriminator: its instruction set and ABI are
right, but load/store scheduling differs. It may come from a separately built
runtime/helper source or expose a host-compiler patch difference; it is not a
reason to discard the three complete byte matches and one relocation-only
match.

The preserved Pro 5 `mwcrtl.lib` independently supplies seven complete exact
functions in the stock target, totaling 693 bytes. They include
`___throw_catch_compare` (287 bytes), `__rt_modu64@16` (125),
`__rt_divu64@16` (113), `__chkstk` (55), three more 64-bit helpers (92 bytes
combined), and no ignored relocations. Together with the official CMU Pro 5.3
fingerprint, this establishes CodeWarrior Pro 5 Win32/x86 2.3 as the validated
host family and a productive matching compiler. It does not yet prove that the
May 1999 executable is the exact minor build used for the April 2001 target.

The first reconstructed subsystem pass compiled `src/backend/Registers.c` at
`-O4,p -inline auto`. It produced six instruction-exact functions after normal
absolute-address relocations were excluded: all three `Coloring_*ColorMask`
functions and all three `Registers_Available*` functions. The initial mask
spelling used `reg < register_count`; measurement exposed the target's
`reg <= last_register` spelling through `cmp last; jle`. Correcting that helper
contract brought the six-function comparable score from 95.24% to 100%.

The reset/setup functions remain a useful exact-minor discriminator. Both a
shared inline helper and six direct two-statement bodies produce the same
extra EBP frame with this 2.3 candidate, while retail is frameless. Because the
source-structure experiment was neutral, the shared helper was retained and
the mismatch is classified as a compiler/header/option difference rather than
papered over with duplicate code.

## Confirmation standard

Do not mark the host compiler or linker confirmed from dates, product names, or
PE section names alone. Confirmation requires candidate binaries and a
reproducible comparison:

1. Run each candidate's version command and preserve its exact hashes.
2. Compile the probe corpus as Win32/i386 objects with recorded command lines.
3. Compare instruction selection, calling convention, exception metadata,
   object sections, symbol decoration, and debug records to the target.
4. Recover a small leaf translation unit from the target and compare candidate
   output with objdiff.
5. Identify the linker separately by reproducing PE section order, alignment,
   imports, CRT/TLS layout, relocations, and entry-point startup.

Downloaded objects and libraries are parsed as untrusted data. Third-party
executables and DLLs are not run on the host or through host Wine/Wibo. If a
candidate compiler must be executed, it must run in a fresh disposable sandbox
with networking disabled, a read-only root filesystem and input mounts,
dropped capabilities, `no-new-privileges`, process and resource limits, and
only a dedicated scratch directory writable. Record the input hash, exact
command, container image identity, and sandbox options with every result.

The next candidate should be the Pro 5.3 x86 update, if a sufficiently
well-provenanced Windows copy can be found. It can distinguish an exact host
minor version from stable 2.3-family code generation. A Pro 6 candidate remains
useful as a boundary test, but the contemporary Metrowerks statement means it
must not be assumed to be the bootstrap compiler.

## Focused code-generation probes

`probes/host/codegen.c` captures five small leaf functions recovered from the
stock executable. They were selected because their target functions contain no
calls and expose code-generator decisions with little frontend ambiguity:

| Probe | Target | Distinguishing behavior |
| --- | --- | --- |
| `absolute_int` | `0x00420a10` | branch and `NEG`, with a separate return on each path |
| `short_predecessor` | `0x00412ed0` | 16-bit test followed by explicit sign extension |
| `initialize_four_words` | `0x00428000` | four scalar stores and an 8-bit Boolean return written only to `AL` |
| `xor_64` | `0x00474e10` | mutates argument homes, then returns through `EDX:EAX` |
| `test_bit` | `0x004bfe60` | signed index split using `AND 15` and arithmetic shift by four |
| `_jzero_far` fingerprint | `0x00441db0` | exact 15-byte Pro 5.3 library match using `REP STOSB` |

The C spelling is still a hypothesis; a mismatch must first be classified as a
source-shape, calling-convention, optimization-level, or compiler-version
difference. A version should only be rejected after ordinary source variants
and optimization levels have been checked. The `initialize_four_words` Boolean
return and `xor_64` calling convention are especially useful for confirming
that a candidate is from the right Win32/x86 ABI family before doing detailed
matching.

## Reproducing the negative self-host test

Run the stock compiler's complete help and inspect `-processor`; then compile:

```text
probes/host/minimal.c
```

Record both the compiler command and the resulting object's machine type, then
verify the distinction with:

```sh
python3 tools/architecture.py \
  orig/GC_1_2_5/mwcceppc.exe build/self-host-probe.o
```

The observed result is PE/i386 for the compiler and big-endian ELF/PowerPC for
the object. The local executable is proprietary, so neither it nor generated
candidate toolchains belongs in this repository.

## Reproducing host-candidate calibration

Verify the official CMU Pro 5.3 source and binary packages and scan the library
directly from its ZIP, without extracting or executing it:

```sh
python3 tools/host_calibration.py \
  --config config/GC_1_2_5/config.json \
  --binary-package /path/to/CW5_Win32_CMUgraphics15_binary_console.zip \
  --source-package /path/to/CW5_Win32_CMUgraphics15_source.zip \
  --output build/GC_1_2_5/cmu-cw53-calibration.json
```

After independently acquiring and extracting the candidate, compile
`probes/host/codegen.c` with `-O4,p` only through the sandbox runner. Then
measure it against the configured stock PE:

```sh
python3 tools/host_probe_match.py \
  --config config/GC_1_2_5/config.json \
  --object /dedicated/output/codegen.obj \
  --output build/GC_1_2_5/host-probe-match.json
```

The runner requires expected SHA-256 values for both the compiler and its
Win32 compatibility runner. See `docs/CAPTURE_EXPERIMENTS.md` for the audited
invocation shape. Candidate binaries, runtime libraries, generated objects,
and reports containing local paths remain local-only.

Real subsystem probes use the declaration-only headers under
`probes/host/include`, `-inline auto`, and the compiler's `-i-` delimiter to
place those headers on the system-include side. The sandbox runner's
`--expect-output` option is mandatory for compile experiments because this
driver can return zero after reporting a compilation abort.

The first `CodeMotion.c` subsystem probe uses
`config/host_copt_probe_targets.json`. At `-O4,p -inline auto`, its three
functions measure 0.00%, 80.21%, and 20.69% over positional comparable bytes.
Both wrappers have the target operation sequence after removing the
candidate-only EBP frame. The central setup routine reproduces the complete
eight-bitset allocation loop and its four final analysis calls; its remaining
body differences are localized to the initial collection walk and assignment
folding. This is useful matching evidence without promoting the candidate to
the exact host minor version.

The same probe now includes three recursive tree walkers. Their control flow
and node actions are exact at the source level, but recursive-inlining depth
separates this candidate from retail: automatic inlining expands seven action
levels and retail expands eight. The documented `level=8` option does not
alter the object, while an explicit source `inline` qualifier expands eleven
levels. This is a useful exact-minor fingerprint: preserve idiomatic recursion
and treat the remaining body-size difference as host lineage evidence rather
than manufacturing a manual unroll.

The adjacent nonrecursive node summarizer provides a same-translation-unit
positive control: `COpt_00521bb0` matches all 344 instruction bytes at the
same flags. Its target extent contains eight trailing alignment bytes. The
result shows that the recursive-depth and EBP-frame differences are localized
compiler fingerprints, not a general inability of this candidate to reproduce
the retail optimizer.

`COpt_00524b20` supplies another call-bearing control: its complete tree-insert
operation sequence aligns after removing the candidate's eight-byte EBP-frame
overhead. Retail begins the separate lookup routine at `0x00524b90`; the
insert is 112 bytes, not the full 176-byte range to the next previously named
function.

## Sources

- Richard Atwell, “CodeWarrior Version 6.0,” *MacTech*, February 2001:
  https://preserve.mactech.com/articles/mactech/Vol.17/17.02/Feb01FactoryFloor/index.html
- CodeWarrior Pro 6 review describing bundled x86 development tools:
  https://preserve.mactech.com/articles/mactech/Vol.17/17.01/CodeWarrior6/index.html
- The official Carnegie Mellon Graphics Lab package corpus is identified by
  filenames and hashes in `(manifest withheld)`; acquisition details
  are intentionally retained only on the local workstation.
