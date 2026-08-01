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
vector (`0x0058849a`), floating-point (`0x0058846e`), then general-purpose
(`0x0058846c`). Each class can enter a retry loop that invokes a spill-code
path before coloring is attempted again.

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
