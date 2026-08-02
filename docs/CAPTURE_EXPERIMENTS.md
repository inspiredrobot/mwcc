# Dynamic capture experiments

Dynamic results are recorded with the exact input identities and sandbox shape
so they can be reproduced without treating an untrusted compiler as a host
tool.

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
