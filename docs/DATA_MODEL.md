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
