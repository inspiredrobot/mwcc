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

The first candidates should be the immediately preceding CodeWarrior Win32/x86
compiler releases. A Pro 6 candidate must also be tested, but the contemporary
Metrowerks statement means it must not be assumed to be the bootstrap compiler.

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

## Sources

- Richard Atwell, “CodeWarrior Version 6.0,” *MacTech*, February 2001:
  https://preserve.mactech.com/articles/mactech/Vol.17/17.02/Feb01FactoryFloor/index.html
- CodeWarrior Pro 6 review describing bundled x86 development tools:
  https://preserve.mactech.com/articles/mactech/Vol.17/17.01/CodeWarrior6/index.html
- Carnegie Mellon Graphics Lab download archive labeling the Win32 packages by
  CodeWarrior release: https://www.cs.cmu.edu/~cm-gfxpkg/download.html
