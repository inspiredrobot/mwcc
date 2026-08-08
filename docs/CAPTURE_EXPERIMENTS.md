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

The adjacent `COpt_00524b90` lookup matches all 56 instruction bytes at the
same flags. The remaining eight bytes in its 64-byte extent are alignment.

The first `COpt_005248c0` reconstruction used semantic helper expressions and
measured 802 candidate bytes versus 608 target bytes at 7.62%. Repeating the
target's short-circuit type tests directly reduced the candidate to 690 bytes
and improved the match to 11.92%. Its switch already lowers to retail's exact
subtract-and-range opcode dispatch. The remaining bulk is concentrated in the
candidate-only EBP frame and repeated Boolean/epilogue materialization around
early returns, so the direct 690-byte form is retained as the cleaner and more
target-shaped baseline.

`COpt_005246d0` has a 496-byte target extent, not 1,104 bytes; the latter
includes `COpt_005248c0`. A shared object-eligibility helper produced a
599-byte census and also regressed the predicate from 690 to 738 bytes.
Restoring the repeated target-shaped short-circuit logic reduced the census to
514 bytes at 12.62% comparable match and restored the predicate to 690 bytes.
This rejects source-level helper sharing here: retail duplicates an inlined
eligibility expression, and that duplication affects the Boolean/register
webs in both callers.

The first `COpt_005240b0` form factored six register-head initializations
through one helper. MWCC inlined and unrolled every zeroing loop eight-wide,
producing 1,991 bytes and 3.53% comparable match. Spelling the three paired
GPR/FPR/vector allocations and scalar loops directly matches retail's visible
structure, reduces the candidate to 1,479 bytes against a 1,568-byte target,
and improves the match to 8.50%. The direct initialization form is retained;
the smaller remaining deficit is localized to repeated entry/link emission.

The first frontend probe reconstructs the `COptimizer.c` expression walk at
`0x004beda0` and its shared object-use helper at `0x004beef0`. The validated
candidate produces 346/95 bytes against retail's 319/80, with positional
comparable scores of 6.77% and 3.13%. This low positional score is explained,
not merely observed: both candidate functions add an EBP frame, the recursive
walk repeats the larger frame epilogue at its returns, and the helper adds one
alignment block. After accounting for those shifts, the dispatch, recursive
edges, inlined object-use bodies, global updates, and early returns align with
retail. The idiomatic shared-helper source is retained because retail itself
keeps the same helper out of line at `0x004beef0` and inlines it at both walk
sites.

`COpt_00524d90` was measured during reconstruction. A first shared-head
definition-kill loop compiled to 628 bytes versus the 736-byte retail extent;
disassembly showed that retail has four distinct kill loops for GPR, FPR,
vector, and object entries. Restoring those semantic arms produced 790 bytes,
revealing a second specific difference: the candidate inlined the otherwise
instruction-exact object-tree lookup while retail called it. A callee-side
`dont_inline` boundary reduced the retained candidate to 757 bytes and 9.19%
positional comparable match. The remaining 21-byte extent difference and
positional shift are dominated by the familiar candidate-only EBP frame and
extra stack homes; the fixed-point scan, eligibility call order, four transfer
arms, and out-of-line lookup now follow retail structure.

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

The grhomerun follow-up expanded the generated site catalog from seven to 26
described allocation sites. The new entries cover conversion and scalar-FP
lowering, indexed-load forcing, Boolean constants, rotate-mask and FNEG
lowering, ABI return-value carriers, and two loop-optimization sites. Every
description carries an explicit confirmed/inferred evidence field and an
address-backed source note. The object-backed allocator wrappers are labeled
at export time as well, so existing captures gain those names without being
rerun. A fresh capture additionally records the full coalescing-parent map and
per-stage virtual-register counter intervals.
It also retains the initial object-stratum limit written immediately after the
first object-preallocation pass, a static landmark used by a later optimizer.

QEMU user-mode's GDB stub rejected even one hardware data watchpoint. A generic
"too many hardware breakpoints/watchpoints" diagnostic was returned both for
three counters and for a GPR-only trial. Counter watchpoints are therefore not
a usable fallback in this sandbox. Version-verified software breakpoints at
the static increment sites plus the reset boundary are both faster and proven
complete for this capture.

### CursorThink loop-motion decision

A second exact-compiler capture at the same source commit records every
instruction considered by `COpt_00524d90` and each result in its short-circuit
predicate chain. It wrote 1,570 events to
`code-motion-0015.json`. Use the checked-in decoder to select the load by its
constant payload:

```sh
python3 tools/explain_code_motion.py code-motion-0015.json --constant=-2.2
```

The candidate's `-2.2f` is event 512, an LFS defining virtual FPR 267 in block
462 (execution weight 8) of a 258-instruction loop node. The decisions are
`00526d80=1`, `00526b50=1`, `005266e0=1`, and `00526500=0`; the final result is
"moved via direct path." The load moves to optimized block 688, whose execution
weight is 1.

This falsifies two earlier hypotheses for this case. Register pressure cannot
be the cause because loop motion runs before register allocation, and this
path contains no allocator query. Block-frequency profitability is not the
cause either: the instruction is accepted by the ordinary direct legality and
invariance chain, not the special `00525fc0` fallback. The retail/candidate
difference must therefore enter through source control flow, frontend
provenance, or one of the direct predicate inputs.

Target branch inspection found one such source error in the PR 3001
reconstruction. Held-slider and cursor-state failures skip the character-kind
toggle but still reach the later team/button logic; they do not continue the
outer loop. A nonzero tag state does continue the loop. Correcting those branch
destinations raises `mnCharSel_CursorThink` from 94.8861% to 94.9820%. The
candidate still hoists `-2.2f`, so this is a verified semantic repair and a
narrower starting point rather than a claim that the residual is solved.

## TextDraw semantic-web and stack-frame validation

Date: 2026-08-08

- Melee source commit:
  `(rev withheld)`;
- `textdraw.c` SHA-256:
  `465f740f1326cad57c6767873679d7ab48d350e5ed577c283e878eefac54334e`;
- MWCC tooling tree committed as `(local rev)`;
- compiler SHA-256:
  `ccf4b465cec73b5aae9c5c5543dcf8cda8a62aba246f89e2e0b200d742f2e55c`;
- Wibo SHA-256:
  `8a8490a6172aa4f0f6ddcadb144ca96f51da6e90e6648ce9adaf4f6babb6e00b`;
- container image ID:
  `sha256:8e8ba9b4718eefbf68b585faff84504dabfd2c90293e25e3bc7b18ded0c475eb`;
- output: function-index 7 snapshots in the dedicated
  `/private/tmp/mwcc-textdraw-stack-0808.UgmJEH` capture mount.

The GDB command file selected `mwcc-auto-capture /capture 7 ninji`. The exact
sandbox and compile command was:

```sh
docker run --rm --platform linux/arm64 --pull never --network none \
  --read-only --cap-drop ALL --security-opt no-new-privileges \
  --pids-limit 128 --memory 2g --cpus 2 \
  --tmpfs /tmp:rw,noexec,nosuid,nodev,size=128m -e HOME=/tmp \
  -v ~/etc/mwcc:/mwcc:ro \
  -v /private/tmp/mwcc-textdraw-stack-0808.UgmJEH:/capture:rw \
  -v ~/etc/melee:/melee:ro \
  -v ~/etc/melee/build/wibo_old:/input/wibo_old:ro \
  -v ~/etc/melee/build/compilers/GC/1.2.5n/mwcceppc.exe:/input/mwcceppc.exe:ro \
  mwcc-debugger:arm64 /bin/sh -c \
  'cd /melee && qemu-i386 -g 1234 /input/wibo_old \
  /input/mwcceppc.exe -nowraplines -cwd source -Cpp_exceptions off \
  -proc gekko -fp hardware -align powerpc -nosyspath -fp_contract on \
  -O4,p -multibyte -enum int -nodefaults -inline auto \
  -pragma "cats off" -pragma "warn_notinlined off" -RTTI off -str reuse \
  -DBUILD_VERSION=0 -DVERSION_GALE01 -i src -i src/MSL -i src/Runtime \
  -i extern/dolphin/include -i src/melee -i src/melee/ft/chara \
  -i src/sysdolphin -c src/melee/if/textdraw.c \
  -o /capture/textdraw.o & \
  gdb-multiarch -q -x /capture/capture.gdb'
```

The new hooks recorded 15 addressed-object allocations at `0x004ac4a0` and
11 checkpoints through frame finalization at `0x004ac240`. The four live
four-byte color objects have local-band/final-SP offsets `+4/+0x0c`,
`+0x0c/+0x14`, `+0x10/+0x18`, and `+0x18/+0x20`. Each allocation has size and
alignment four. The finalizer adds an eight-byte linkage base, retains a
`0x60`-byte local-object area, adds `0x20` bytes of GPR saves and eight bytes
of FPR saves, and finishes at the observed `0x90`-byte frame.

This explains the current `DevText_Draw` diff directly. Its only mismatches
are the text-color `ADDI` and `STW`, both at candidate `sp+0x20` versus retail
`sp+0x10`; the allocation trace identifies candidate local slot `+0x18` and
the final eight-byte SP base. The four color objects intentionally remain one
ambiguous semantic group because they have the same compiler name, type, and
live `ADDI`/`STW` use signature. The report preserves that ambiguity while
still exposing allocation order and exact frame-band ownership.

The register-web aligner was separately checked on the saved `fn_8001EBF0`
baseline/carrier captures. It maps `gpr:35 -> gpr:39` at score 0.84 and
`gpr:39 -> gpr:38` at score 0.93, both high confidence, along with the
surrounding `36 -> 35`, `37 -> 36`, and `38 -> 37` shifts. All 30 GPR webs
align with no ambiguous, inserted, or deleted web, despite the aggregate
origin comparison having reported no changes.

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
