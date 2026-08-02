# Working backend data model

This file separates exact target observations from provisional layouts. Fields
should be promoted into C headers only after their offsets are confirmed by
multiple target functions.

## Exact observations

At `0x004ccf10`, the scheduler dependency helper walks a packed operand array
with a stride of 12 bytes. The containing instruction has an operand count at
offset `0x1a` and inline operands beginning at `0x1c`. Within each operand:

- offset `0x00`: operand kind byte;
- offset `0x01`: operand flags byte;
- offset `0x02`: 16-bit register number for register operands;
- offset `0x06`: a 32-bit pointer/value used by memory-like operands.

The helper distinguishes operand kinds 0, 1, 2, 3, 4, 5, and 9 and constructs
dependencies against separate register-class tables. These facts are visible
in this target's instructions and are not imported from an external debugger.

At `0x004cdef0`, coloring operates on three class/count globals in this order:
vector (`0x0058849a`, class 9), general-purpose (`0x0058846e`, class 0), then
floating-point (`0x0058846c`, class 1). The nearby `VR`, `GPR`, and `FPR`
diagnostic strings independently establish this mapping. Each class can enter
a retry loop that invokes a spill-code path before coloring is attempted
again.

`Registers_GetInfo` at `0x004c1720` allocates a zeroed `0x2c`-byte record for
object kinds 0 and 2, while kind 1 uses an existing record. Together with
`Coloring_SetupFPRs` at `0x004ce710`, this confirms the record's physical
register field at `0x24` and its GPR/FPR discriminator at `0x28`.

The eight binding/allocation functions at `0x004c1b40` through `0x004c2280`
add independent evidence for these fields and confirm:

- a secondary physical register at `RegisterInfo + 0x26` for paired GPR
  values;
- the FPR-class byte at `RegisterInfo + 0x28`;
- the vector-class byte at `RegisterInfo + 0x2a`;
- an object type pointer at `CompilerObject + 0x0e`, whose first byte can
  override the FPR-class bit when target option `0x00584244` is enabled;
- 32-byte physical-use tables at `0x00581310`, `0x00581330`, and `0x00581350`
  for GPR, FPR, and vector registers respectively.

The binding functions rescan class-specific physical ranges whenever a new
register is marked used. They derive both the save span and the remaining free
saved-register count. The allocation functions either consume monotonically
increasing virtual-register numbers or search the physical-use tables from
register 31 downward. These shared mechanics are reconstructed in
`src/backend/Registers.c` and covered by a host-side behavioral test.

The 15 state helpers at `0x004c14d0` through `0x004c1b20` complete the
physical-register state used by color selection. Before a class is allocated,
its 32-byte physical-use table and save span are copied into shared working
storage at `0x00581372` and `0x00581370`. Color selection restores that
snapshot before each attempt. Initial color masks contain free volatile
registers only: GPR r0-r12, FPR f0-f13, and VR vr0-vr19. When no initial color
is available, the class-specific claim helper searches downward from register
31, stopping before r14, f14, or vr20 respectively, and records the claimed
register through the ordinary binding path. The availability helpers count
all zero entries in the corresponding 32-byte table. These exact bounds and
copy directions are confirmed by the stock executable and behaviorally tested.

The three coloring setup routines at `0x004ce5f0`, `0x004ce710`, and
`0x004ce850` confirm the interference-node layout through its inline neighbor
list:

- `+0x00`: temporary list link used during simplify/select;
- `+0x04`: associated compiler object for precolored and virtual nodes;
- `+0x08`: spill-cost numerator;
- `+0x0c`: virtual-register number;
- `+0x0e`: current interference degree;
- `+0x10`: selected physical register;
- `+0x12`: allocator flags;
- `+0x14`: neighbor count;
- `+0x16`: inline array of 16-bit neighbor indices.

Setup assigns colors 0 through 31 to the physical nodes and then seeds them
from two shared object lists. Vector and FPR membership use the `RegisterInfo`
class bytes. GPR membership also recognizes paired values from the object type
kind and size at `CompilerType + 0x02`; flags `0x20` and `0x10` identify the
first and second halves. This shared precolor model is behaviorally tested.

The simplify/select/commit routines at `0x004ce400`, `0x004ce2d0`, and
`0x004ce1a0` establish the allocator’s central decisions:

1. repeatedly remove every active virtual node whose degree is less than the
   number of available colors, decrementing each neighbor’s degree;
2. when no such node remains, rank spill candidates by `spill_cost / degree`
   (with separate fixed scores for protected nodes), remove the lowest score,
   and simplify again;
3. pop the resulting stack, remove every already-colored neighbor from the
   class color mask, and choose the lowest remaining color bit;
4. if the mask is empty, claim another physical color for the class or set the
   node’s spill flag when no register is available;
5. resolve coalesced color aliases, rewrite matching 12-byte PCode operands,
   remove newly redundant instructions, and commit primary or secondary colors
   to `RegisterInfo`.

The target rewrites PCode before resolving coalesced aliases for object
metadata. Consequently, coalescing must already have redirected live PCode
operands to the surviving graph node; the commit pass does not repair an
operand that still names a coalesced-away node. A behavioral-test failure made
this ordering constraint explicit and the disassembly at `0x004ce1a0`
confirms it.

This confirms a 12-byte `PCodeOperand`, an instruction operand array at
`PCodeInstruction + 0x1c`, and a block instruction-list pointer at
`PCodeBlock + 0x14`. The high-level algorithm is covered by tests for
low-degree simplification, minimum spill-cost selection, lowest-bit coloring,
color exhaustion, coalesced aliases, PCode rewriting, and paired-register
commit behavior.

`SpillCode_ComputeSpillCosts` at `0x00532790` defines the numerator used by
spill ranking. For each instruction operand in the selected class, it adds
twice the block execution weight for operand flag `0x01` and once the weight
for flag `0x02`; an operand carrying both flags receives three times the
weight. The block weight is stored at `PCodeBlock + 0x28`. Option byte
`0x005842e2` replaces every block weight with one, providing an unweighted
mode. The behavior is covered by weighted and uniform test cases.

`SpillCode_BuildInterference` at `0x00530a00` confirms a six-stage graph
pipeline: initialization, two construction/coalescing stages, an optional
class-specific dump, and two finalization stages. The internal roles retain
address-suffixed names until their individual state transitions are recovered.

The four pointers in each 16-byte `PCodeBlockLiveness` record are confirmed as
`use`, `def`, `live_in`, and `live_out`. `SpillCode_BuildLocalLiveness` at
`0x00530530` walks instructions forward. A use not preceded by a definition is
added to the block’s upward-exposed `use` set; a definition not preceded by a
use is added to `def`. `SpillCode_SolveLiveness` at `0x00530410` walks a
depth-first block order backward until stable, unions successor `live_in` sets
to form `live_out`, and applies `live_in = use | (live_out & ~def)`. Behavioral
tests cover local-set construction, successor propagation, and definition
kills.

`SpillCode_InitializeLiveness` at `0x005301b0` establishes the whole dataflow
problem. It builds a depth-first block order, allocates four cleared bitsets per
block, constructs the local sets, seeds result-register uses required by the
function’s ABI return type, then invokes the fixed-point solver. The result type
is reached through pointers at `PCodeFunction + 0x0e` and its signature
`+0x0e`; the type’s kind, size, and subtype are at offsets `0x00`, `0x02`, and
`0x0e`.

The exact return seeds are class dependent. GPR-class scalar returns use r3 and
8-byte integer-like values additionally use r4. Eligible aggregate returns use
the same pair, FPR-class type kind 2 uses f1, and VR-class type kind 4 with
subtype 4 through 14 uses vr2. These are inserted as upward-exposed uses in the
return block before liveness propagation. Tests cover allocation of all four
sets and the two-register GPR return case.

Before the ordinary backward last-use transfer, `SpillCode_IsDeadInstruction`
at `0x00530050` recognizes definitions that can be deleted. Instruction flags
matching mask `0x00020434` and context flags `0x03` are barriers. GPR, FPR, and
VR definitions are removable only while analyzing their own class and only
when their destination is not live; SPR and condition-register definitions are
always retained. Global option byte `0x005842e1` enables the deletion after all
operands pass those checks. The instruction context pointer is at
`PCodeInstruction + 0x08`, with its barrier flags at context offset `0x2e`.
Tests cover dead, live, context-protected, and instruction-protected
definitions.

Four of those internal stages are now identified. `SpillCode_MarkLastUses` at
`0x00530a80` initializes a bitset from each block’s live-out state, then walks
the block backward through the instruction link at `PCodeBlock + 0x18`.
Definitions (operand flag `0x02`) leave the live set; uses (flag `0x01`) enter
it. A use not already present receives flag `0x04`, establishing it as the
forward last use. The block index at `+0x1c` selects a live-out record whose
pointer is at offset `0x0c`.

`SpillCode_MaterializeGraph` at `0x00530c00` converts a triangular interference
bit matrix into the variable-sized node layout above. It records every neighbor
index, initializes degree from the neighbor count, then applies a 16-bit
coalescing-parent map. A non-root node receives flag `0x04` and stores its root
index in the physical-register field; the root receives flag `0x08`. Tests
cover last-use marking, live-out preservation, neighbor materialization,
degrees, and coalescing-root resolution.

`SpillCode_ConstructInterference` at `0x00531290` allocates and clears the
triangular matrix, makes physical registers 0 through 31 a clique, and then
walks every block backward from its live-out set. Each definition interferes
with every currently live register. Instruction flag `0x0800` excludes the
register in operand 1 from that edge set, preserving the source/destination
coalescing opportunity. Uses enter the live set after definition edges are
created and receive the same last-use marker described above.

The GPR path also records target constraints. Instructions selected by flag
mask `0x0018` constrain operand 1, and flag `0x8000` additionally makes
operands 0 and 1 interfere. Opcodes `0x3f` and `0x42` constrain operand 1;
opcodes `0x37` through `0x3b` constrain operand 0. Flag `0x0020` applies a
fixed-register constraint to operands 50 onward by making each interfere with
physical GPRs 3 through 12. A constrained virtual register is represented by
the otherwise-unused diagonal bit at index `reg * reg / 2`; this special index
is not the ordinary off-diagonal pair formula. Tests cover the physical clique,
live-definition edges, copy-source exclusion, and diagonal encoding.

`SpillCode_CoalesceCopies` at `0x00530e00` identifies the class-specific copy
opcodes (`0x8b`, `0x9e`, and `0x18e` for GPR, FPR, and VR respectively). A copy
whose roots do not interfere may be removed; the lower-numbered root survives,
and every interference edge of the discarded root is transferred to it.
Physical registers below 32 are always eligible, while virtual-register pairs
must both fall inside the class-specific coalescing range. A final walk rewrites
every operand of the selected class to its canonical root. This directly
explains the canonical-PCode invariant required by `Coloring_CommitAssignments`.
Tests cover copy removal, root selection, edge transfer, and operand rewriting.

The minimal validated layouts live in `include/mwcc/backend_types.h`; padding
remains explicit until more fields are understood.

## Provisional GC/1.x hypotheses

An external GC/1.1 debugger suggests the following shapes, which are useful
search hypotheses but are not yet validated for GC/1.2.5:

- a PCode instruction header of `0x1c` bytes followed by 12-byte operands;
- a basic-block structure containing linked-list pointers, predecessor and
  successor lists, an instruction pointer, index, loop weight, instruction
  count, and flags;
- an interference-graph node containing next/object pointers, cost, virtual
  and physical register numbers, flags, a neighbor count, then inline 16-bit
  neighbor indices;
- object records containing type/name links and a stack offset.

Do not encode those provisional offsets into source structs yet. Validate them
by collecting offset use from the exact functions in `Coloring.c`,
`SpillCode.c`, `StackFrameEABI.c`, and the PCode listing routines.

## Layout-validation method

For a candidate structure, maintain an offset table with every reading and
writing function. Require at least two independent consumers, or one consumer
plus an allocation/initialization site, before assigning a semantic field
name. Record width and signedness separately; Ghidra's inferred C type is not
evidence by itself.
