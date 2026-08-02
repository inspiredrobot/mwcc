/*
 * PCode.c
 *
 * Working entry points:
 *   0x0049d270  PCode_CloneInstruction
 *
 * The clone deliberately copies only the opcode, flags, operand count, and
 * operands. List links and context belong to the insertion site. Two flag bits
 * reserve an additional operand-sized slot for a subset of instructions; the
 * semantic reason for that hidden storage is not yet established.
 */

#include "mwcc/PCode.h"

extern void* PCode_Allocate(unsigned int size); /* 0x00441fa0 */

/* 0x0049d270; functionally equivalent; binary match unmeasured. */
PCodeInstruction* PCode_CloneInstruction(PCodeInstruction* source)
{
    PCodeInstruction* clone;
    unsigned int size;
    int index;

    size = sizeof(PCodeInstruction) +
           (unsigned int) source->operand_count * sizeof(PCodeOperand);
    if ((source->flags & PCodeInstruction_CloneExtraOperand) != 0 &&
        (source->flags & PCodeInstruction_CloneExtraOperandExcluded) == 0)
    {
        size += sizeof(PCodeOperand);
    }
    clone = PCode_Allocate(size);
    clone->opcode = source->opcode;
    clone->flags = source->flags;
    clone->operand_count = source->operand_count;
    for (index = 0; index < source->operand_count; index++) {
        clone->operands[index] = source->operands[index];
    }
    return clone;
}
