# MWCC GC/1.2.5(n) stage-2 instruction scheduler — exact model

Recovered 2026-08-14 from the stock binary (byte-identical in 1.2.5n) and
validated end-to-end: a Python simulator reproduces the full 54-instruction
scheduled stream of `grStadium_801D435C`'s unrolled sum block 54/54 rows, plus
every other >2-insn block of that function. Simulator:
`(case study withheld)` (also `build/schedsim_mwcc.py` in the melee
repo). Live-trace recipe (emit/cycle breakpoints) in the melee session notes
`notes/HANDOFF_GRPSTADIUM_801D435C.md`.

## Entry points

- `0x4ccae0` driver: selects the machine model into `[0x581b80]` from the CPU
  byte `[0x584224]`, then calls the per-block scheduler for every block with
  more than 2 instructions (count word at block+0x2c; flags at +0x2e: skip if
  `&3` unless the final-pass arg is set, `&8` = already scheduled).
- **Model selection quirk: `-proc gekko` sets CPU byte 8, which has NO case in
  the driver switch — melee compiles schedule with the DEFAULT model at
  `0x574d70`**, not the "gekko-ish" table at 0x577640 (CPU 7) nor 0x578e30
  (CPU 9). Verified live in gdb (`model=574d70`, `flag230=0`, `cpu_byte=8`).
- `0x4ccbf0` per-block scheduler, `0x4ccf10` dependence builder,
  `0x4cd7c0` register deps, `0x4cd4a0`/`0x4cd650` memory deps (object /
  volatile), `0x4cd2f0` wildcard memory deps (insn flag 0x40),
  `0x4cd910` edge adder, `0x4ccdc0` pick function.
- `0x4cca50` (dependence test used by some models' can_issue) **always returns
  0** — the match path and the exhaust path both `xor eax,eax; ret`. Dead code
  in the shipped build.

## Default machine model (0x574d70) — what melee actually uses

- dword[0]=2: issue width 2 per cycle. dword[1]=1: register WAR/WAW edge
  latency 0 for GPR/FPR/VR classes (CR/SPR keep producer latency).
- Per-opcode records at `0x574d90 + opcode*6`: `[unit, latency, occupancy,
  stage2cd, stage3cd, serialize]`. Key rows: ADDI/MR/LI/RLWINM = unit1(IU),
  lat 1; LWZ/LFS/STFS = unit2(LSU) lat 2; FADDS/FMULS = unit4(FPU) lat 3;
  FCMPU/FCMPO lat 5; FDIVS lat 18/occ 18; CMPI lat 3; B/BL/BT/BF unit0 lat 0
  **serialize=1** (branches are barriers). Latency +2 if insn flag 0x80;
  LMW/STMW add (operand_count - 2).
- **One functional unit per class** (single IU — at most one ADDI/MR/LI issues
  per cycle). Units 0..7, slot = insn, countdown = occupancy.
- Pipelines: LSU u2→u3 (stage cd from rec[3]), FPU u4→u5→u6 (rec[3], rec[4]);
  FDIV completes directly from u4. Completions mark a 5-entry in-order ring;
  ≤2 retires per cycle; ring size 5 = max in-flight (`slots` init 5).
- can_issue: slots>0, unit[class] free, and a store cannot issue while u3
  (LSU stage 2) holds a store.
- advance() order each cycle: decrement all countdowns → ≤2 in-order retires →
  completions (u1, u3, u6, u7, u0, u4-if-FDIV) → stage moves (u5→u6, u4→u5,
  u2→u3).

## Dependence construction (backward walk)

Nodes are created walking the block's instructions LAST→FIRST (block+0x18 is
the tail, insn+4 is `previous`). Per instruction, per operand in order:

- GPR/FPR/CR/SPR: two per-register lists (later uses A, later defs B).
  A use adds edges use→(later defs) (WAR, lat 0 for GPR/FPR) and prepends to A.
  A def adds def→(later uses) (RAW, lat = def's latency), def→(later defs)
  (WAW, lat 0) and prepends to B. Lists are never pruned, so edges are
  conservative (all later same-register ops, not just the nearest).
  GPR skips: reg 2, reg 13, and reg 0 with no access flags. Registers here are
  VIRTUAL (pre-RA); vregs of named locals/temps as usual.
- Memory (kind 4/5 operands, insn flags & 0x18, object != 0): per-object
  load/store lists; load→(later same-object stores) lat = load latency;
  store→(later same-object loads/stores) lat = store latency; object identity
  is pointer equality, with an alias predicate for object-0 entries.
- **Wildcard pass (insn flag 0x40, set on every pointer-based load/store)**:
  cd2f0 adds edges to/from ALL later wildcard-ish entries and inserts the insn
  with object 0. Pointer-based loops therefore serialize store→every-later-load
  with latency 2 — this is why grStadium's unrolled table loads issue exactly
  STFS+2 apart.
- Barriers: insn flags & 0x420 or serialize byte → edges from the barrier to
  every later instruction and from every earlier instruction to the barrier.
  A no-successor node gets an edge to the block terminator (flag & 4).

**Heights** (`node+0x16`): initialized to own latency; every edge add updates
`from.h = max(from.h, edge_lat + to.h)`. Because the walk is backward, `to.h`
is final when the edge is added ⇒ h = true critical-path height to block end.
`0x581b84` = global running max height — **never reset between blocks or
functions** (only site is the max update; deadlines shift uniformly, ties are
unaffected). Deadline (`node+0x14`) = `0x581b84 - h` (ALAP-style due cycle).

## The pick rule (0x4ccdc0) — 4-level tie-break

Scan candidates in TEXTUAL order among unissued nodes; issuable = pred count 0,
ready ≤ cycle, can_issue. First issuable becomes `best`; a later candidate
`c` replaces it only by strict wins, gated by
`(cycle < best.deadline || c.deadline <= cycle)`:

1. **Urgency**: `c` due (deadline ≤ cycle) while `best` not due → c wins.
2. **Release count**: # of successors with pred count == 1 (i.e. that this
   issue would make ready). Strictly more wins.
3. **Height**: strictly larger h wins.
4. **Descriptor byte 9** (0x5654b0 + op*16 + 9; only when vregs in use,
   `[0x587648] != 0`): strictly smaller wins. MR=0, STFS/CMP/FCMP=1,
   ADDI/FADDS=2, LFS/LWZ=3, LI/LIS=4.
5. Otherwise the earlier-textual instruction keeps the slot.

Ready times update at issue: `succ.ready = max(succ.ready, cycle + edge_lat)`.

## Why grStadium_801D435C's last unrolled iteration mis-schedules

Steady state: each clone lowers as [LFS acc; MR temp,IV; ADDI IV+=16;
LFS table,12(temp); FADDS; STFS] (compound `+=`: acc read lowers first, and
FADDS operand order = materialization order ⇒ srcA=acc, the target encoding).
The scheduler hoists the table load above the acc load in clones 0–7 because
the table load's WAR successor `MR(i+1)` sits at pred-count 1 at the decisive
cycle (release-count win, level 2). In the last clone there is no `MR(9)`:
both loads tie on all four levels (equal h=7, rc=0, byte9 LFS=LFS) and the
acc load wins on textual order. Emission then deletes the dead 9th ADDI and
folds MR/ADDI into displacements, but preserves the load order.

Delta search over the validated simulator: the ONLY single-graph-edit that
produces the target stream is an edge table8→acc8 (any latency), equivalently
inserting a post-load write of the load-temp web (a dead `ADDI/MR` of that
web). No C construct reaches it: tree-level DCE kills dead post-loop
statements, PCode DCE spares only unroller-created IV increments, and those
touch the IV web, not the per-clone copy temp the loads read.

Store→table-load wildcard edges cannot be removed either (pointer-based loads
have object 0 and insert wildcard entries); removing them in the simulator
breaks the steady-state spacing, so the target compile had them too.
