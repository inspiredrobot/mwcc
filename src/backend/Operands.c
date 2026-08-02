/*
 * Operands.c
 *
 * Working entry points:
 *   0x004a0550  Operands_ForceFPR
 *
 * Operand kinds 9 and 10 represent direct and indexed memory values. This
 * routine materializes either form in an FPR, reusing a requested destination
 * when one is supplied and allocating a compiler temporary otherwise.
 */

#include "mwcc/Operands.h"

enum {
    OperandKind_FPR = 5,
    OperandKind_DirectMemory = 9,
    OperandKind_IndexedMemory = 10,
    PCode_LFS = 0x8e,
    PCode_LFSX = 0x90,
    PCode_LFD = 0x92,
    PCode_LFDX = 0x94
};

extern unsigned char gOperandsDebug;   /* 0x00584244 */
extern short gUsedVirtualRegistersFPR; /* 0x0058846c */

extern void Operands_Assert(const char* file, int line); /* 0x00445780 */
extern void Operands_DebugType(int code);                /* 0x0047cbd0 */
extern void Operands_Normalize(Operand* operand);        /* 0x004a0fc0 */
extern void Operands_EmitMemoryInstruction(short opcode, short destination,
                                           short base, CompilerObject* object,
                                           int displacement); /* 0x004a1dd0 */
extern void PCodeUtilities_EmitInstruction(int opcode, ...);  /* 0x004a25d0 */
extern void Operands_PropagateFlags(unsigned int flags);      /* 0x0049cf70 */

static short Operands_AllocateFPR(short requested_register)
{
    if (requested_register != 0) {
        return requested_register;
    }
    return gUsedVirtualRegistersFPR++;
}

/* 0x004a0550; functionally equivalent; binary match unmeasured. */
void Operands_ForceFPR(Operand* operand, CompilerType* type,
                       short requested_register)
{
    short destination;
    short opcode;

    if (gOperandsDebug != 0 && type->kind == 2) {
        Operands_DebugType(0x84);
    }
    Operands_Normalize(operand);

    switch (operand->kind) {
    case OperandKind_FPR:
        destination = operand->reg;
        break;
    case OperandKind_DirectMemory:
        destination = Operands_AllocateFPR(requested_register);
        opcode = type->size == 4 ? PCode_LFS : PCode_LFD;
        Operands_EmitMemoryInstruction(opcode, destination, operand->reg,
                                       operand->object, operand->displacement);
        Operands_PropagateFlags(operand->flags_0a);
        break;
    case OperandKind_IndexedMemory:
        destination = Operands_AllocateFPR(requested_register);
        opcode = type->size == 4 ? PCode_LFSX : PCode_LFDX;
        PCodeUtilities_EmitInstruction(opcode, destination, operand->reg,
                                       operand->secondary_reg);
        Operands_PropagateFlags(operand->flags_0a);
        break;
    default:
        Operands_Assert("Operands.c", 0x320);
        return;
    }

    operand->kind = OperandKind_FPR;
    operand->reg = destination;
}
