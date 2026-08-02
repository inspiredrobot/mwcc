# Dynamic capture experiments

Dynamic results are recorded with the exact input identities and sandbox shape
so they can be reproduced without treating an untrusted compiler as a host
tool.

## CodeWarrior Pro 5 Win32/x86 host calibration

Date: 2026-08-02

- candidate: CodeWarrior Professional Release 5 Windows `mwcc.exe`;
- candidate SHA-256:
  `738dd623e16597922d2784facf5738377370b45cf2e055a6006093d142570c03`;
- Wibo SHA-256:
  `8a8490a6172aa4f0f6ddcadb144ca96f51da6e90e6648ce9adaf4f6babb6e00b`;
- container image ID:
  `sha256:8e8ba9b4718eefbf68b585faff84504dabfd2c90293e25e3bc7b18ded0c475eb`;
- candidate source and extraction hashes: recorded in
  `(manifest withheld)`;
- inputs: read-only `probes/host` mount;
- outputs: a fresh dedicated `/private/tmp/cwpro5-codegen.*` mount.

The version probe and compilation both used `tools/run_host_candidate.py`.
The compile payload after the fixed sandbox arguments was:

```text
qemu-i386 /sandbox/wibo /candidate/mwcc.exe -O4,p \
  -c /input/codegen.c -o /output/codegen.obj
```

The runner generated the complete Docker argument vector. Its enforced shape
was `--pull never`, `--network none`, `--read-only`, `--cap-drop ALL`,
`--security-opt no-new-privileges`, `--pids-limit 64`, `--memory 512m`,
`--cpus 1`, a 64 MiB `noexec,nosuid,nodev` `/tmp`, three read-only mounts, and
one dedicated writable output mount. The host imposed a 30-second timeout.

The version command returned 2.3, built May 26 1999. The `-O4,p` object
reproduced four of five recovered functions exactly after excluding one
ordinary four-byte address relocation. `tools/host_probe_match.py` reports
89.23% raw and 91.27% comparable byte match across all five. See
`docs/HOST_TOOLCHAIN.md` for per-function results and interpretation.

A subsequent `Registers.c` build used `-O4,p -inline auto`, declaration-only
probe headers, and `-i-` to mark their path as a system include directory. Six
functions measured 100% over comparable instruction bytes. The runner now
requires `--expect-output` for compilation because an earlier missing-header
abort demonstrated that this driver can still return status zero.

A `CodeMotion.c` build used the same candidate, flags, headers, runner, and
sandbox shape. The three focused functions measured 0.00%, 80.21%, and 20.69%
over positional comparable bytes. The target sizes are 22, 407, and 59 bytes;
the candidate sizes are 28, 411, and 65 bytes. Instruction inspection shows
that the two wrapper bodies are operation-identical after removing the
candidate-only EBP frame. The 407-byte setup reproduces the complete bitset
allocation loop and final call order. A declaration-order experiment reduced
its score to 75.97% by changing the allocation-state/index registers to
EBX/EDI, so the clean 80.21% form was retained.

The probe was then extended to the recursive walkers at `0x00521a30`,
`0x00524c10`, and `0x00525070`. Plain recursive C produced candidate/target
sizes of 316/382, 316/382, and 340/384 bytes, with comparable positional
matches of 13.73%, 13.73%, and 20.83%. The candidate automatically expands
seven action levels before retaining a recursive call; retail expands eight.
Adding `-inline auto,level=8` did not change a byte. Marking the functions
explicitly `inline` expanded eleven levels instead (496, 496, and 628 bytes)
and reduced the matches to 10.23%, 10.23%, and 12.64%. Moving the per-node
body into a helper reduced expansion to four levels and candidate sizes to
182, 182, and 244 bytes. The simple recursive form is retained because every
source-shape alternative moves away from the exact, independently observed
retail depth.

The adjacent node summarizer at `0x00521bb0` then produced an exact positive
control. Its retained source matches all 344 instruction bytes; the 352-byte
target extent includes eight zero alignment bytes before `0x00521d10`. An
initial named `short opcode` local produced 347 bytes and 22.19% comparable
match. Correcting signed `PCodeBlock +0x2e` handling and the contiguous barrier
test produced 345 bytes and 61.74%. Removing the named opcode local let copy
propagation reuse the dead instruction-flags register as its low-word opcode
value, removed one saved register, and reached 100%. This was a source-object
lifetime difference, not a `register`-keyword effect.

`COpt_00524b20` measures 26.04% over 96 comparable bytes, with candidate and
target extents of 120 and 112 bytes. Disassembly shows the familiar localized
difference: the candidate creates an EBP frame and consequently maps the tree
link/object webs to ESI/EBX, while retail has no frame and uses EBX/EBP. The
pointer comparisons, branch topology, allocation call, 24-byte initialization
order, allocation-list update, and tree-link store otherwise correspond.

## CursorThink optimizer lineage

Date: 2026-08-01

- source worktree commit:
  `(rev withheld)` (clean at capture time);
- compiler SHA-256:
  `ccf4b465cec73b5aae9c5c5543dcf8cda8a62aba246f89e2e0b200d742f2e55c`;
- Wibo SHA-256:
  `8a8490a6172aa4f0f6ddcadb144ca96f51da6e90e6648ce9adaf4f6babb6e00b`;
- container image: local `mwcc-debugger:arm64`;
- output: function-index 15 snapshots and creation trace in the dedicated
  `/private/tmp/mwcc-directalloc-cursor` capture mount.

The GDB command file contained:

```text
set pagination off
set confirm off
set architecture i386
target remote :1234
source /mwcc/tools/gdb/allocator_snapshot.py
mwcc-auto-capture /capture 15 ninji
continue
quit
```

The exact sandbox invocation was:

```sh
docker run --rm --platform linux/arm64 --network none --read-only \
  --cap-drop ALL --security-opt no-new-privileges --pids-limit 128 \
  --memory 2g --cpus 2 --tmpfs /tmp:rw,noexec,nosuid,size=128m \
  -e HOME=/tmp \
  -v /private/tmp/mwcc-gpr:/mwcc:ro \
  -v /private/tmp/mwcc-directalloc-cursor:/capture:rw \
  -v /private/tmp/melee-charsel:/melee:ro \
  -v ~/etc/melee/build/wibo_old:/input/wibo_old:ro \
  -v ~/etc/melee/build/compilers/GC/1.2.5n/mwcceppc.exe:/input/mwcceppc.exe:ro \
  mwcc-debugger:arm64 /bin/sh -c \
  'cd /melee && qemu-i386 -g 1234 /input/wibo_old /input/mwcceppc.exe \
  -nowraplines -cwd source -Cpp_exceptions off -proc gekko -fp hardware \
  -align powerpc -nosyspath -fp_contract on -O4,p -multibyte -enum int \
  -nodefaults -inline auto -pragma "cats off" \
  -pragma "warn_notinlined off" -RTTI off -str reuse -DBUILD_VERSION=0 \
  -DVERSION_GALE01 -i src -i src/MSL -i src/Runtime \
  -i extern/dolphin/include -i src/melee -i src/melee/ft/chara \
  -i src/sysdolphin -c src/melee/mn/mncharsel.c \
  -o /capture/mncharsel.o & \
  gdb-multiarch -q -x /capture/capture.gdb'
```

The capture produced 2,507 initial, 2,365 post-O4, and 2,335 pre-coloring PCode
instructions. Normal constructor events explain 2,315 pre-coloring survivors;
20 `PCode_CloneInstruction` events explain the remainder. All 20 clones retain
live parents, yielding complete instruction provenance for this capture.

The subsequent virtual-register-birth capture accounts for 1,157 allocation
events that reach the target function: 180 object-backed GPRs, 659 direct GPR
temporaries, 75 object-backed FPRs, and 243 direct FPR temporaries. Every one of
the 695 live virtual GPR webs and 273 live virtual FPR webs in pre-coloring
PCode has an origin. `fpr:265` has one and only one origin,
direct increment `0x004a05b7` in the `Operands.c` routine named here
`Operands_ForceFPR`. Static control flow shows that its kind-9 operand path
allocates a destination when no FPR was requested and chooses LFS or LFD from
the operand type size. The captured web's definition is LFD. Its CodeGen-item
identity is `0x409124a0`, shared with the already traced LFD/FCMPO lowering.

An earlier run instrumented the same 32 FPR increment instructions but retained
events only after target selection and reported no direct events. That absence
was not evidence that direct allocation was unused. The corrected capture
retains pre-CodeGen events separately and, more importantly, validates the
entire generated site catalog at runtime. The direct GPRs are allocated before
the target's CodeGen entry, so the trace buffers them between the compiler's
explicit register-counter reset at `0x004c23c0` and the next CodeGen boundary.
Future negative conclusions must check capture-window ownership as well as
breakpoint coverage.

QEMU user-mode's GDB stub rejected even one hardware data watchpoint. A generic
"too many hardware breakpoints/watchpoints" diagnostic was returned both for
three counters and for a GPR-only trial. Counter watchpoints are therefore not
a usable fallback in this sandbox. Version-verified software breakpoints at
the static increment sites plus the reset boundary are both faster and proven
complete for this capture.

## Stock compiler-object snapshot smoke test

Date: 2026-08-01

- source SHA-256:
  `834ac363e4fed9867ff278c79ff3b9d69ee8cb65a58269e0a36bbeb648fc8103`;
- compiler SHA-256:
  `0443b5c02b1aa7b575b61e0e24c4d5ad6bed8fd54cc42de5a2204a5216001914`;
- Wibo and container identities: same as the CursorThink experiment;
- GDB command: `mwcc-auto-capture /capture 1 stock`.

The exact sandbox invocation was:

```sh
docker run --rm --platform linux/arm64 --network none --read-only \
  --cap-drop ALL --security-opt no-new-privileges --pids-limit 128 \
  --memory 1g --cpus 2 --tmpfs /tmp:rw,noexec,nosuid,size=64m \
  -e HOME=/tmp \
  -v /private/tmp/mwcc-gpr:/mwcc:ro \
  -v /private/tmp/mwcc-directalloc-small:/capture:rw \
  -v /private/tmp/mwcc-trace.m3a0nK/minimal.c:/input/minimal.c:ro \
  -v ~/etc/melee/build/wibo_old:/input/wibo_old:ro \
  -v ~/etc/melee/build/compilers/GC/1.2.5/mwcceppc.exe:/input/mwcceppc.exe:ro \
  mwcc-debugger:arm64 /bin/sh -c \
  'qemu-i386 -g 1234 /input/wibo_old /input/mwcceppc.exe \
  -proc gekko -fp hardware -O4,p -c /input/minimal.c \
  -o /capture/minimal.o & \
  gdb-multiarch -q -x /capture/capture.gdb'
```

One of 27 creation operands retained a compiler object. The trace decoded its
tag 5, kind 0, object flags `0x00010001`, type kind 2, size 4, type flags 8,
and subtype 14. The provenance export preserved the same fields and linked all
10 live optimized instructions to origins.
