/*
 * Operands.c
 *
 * Working entry points:
 *   0x004a0550  Operands_ForceFPR
 *   0x004a0ba0  Operands_ForceGPR
 *
 * The force routines turn the operand forms produced by frontend lowering into
 * virtual-register values. Their counter increments are the birth sites used
 * by the register-origin capture tools.
 */

#include "mwcc/Operands.h"

enum {
    OperandKind_GPR = 0,
    OperandKind_Address = 1,
    OperandKind_GPRSum = 2,
    OperandKind_GPRPair = 3,
    OperandKind_Immediate = 4,
    OperandKind_FPR = 5,
    OperandKind_Condition = 7,
    OperandKind_DirectMemory = 9,
    OperandKind_IndexedMemory = 10,
    PCode_LBZ = 0x15,
    PCode_LBZX = 0x17,
    PCode_LHZ = 0x19,
    PCode_LHZX = 0x1b,
    PCode_LHA = 0x1d,
    PCode_LHAX = 0x1f,
    PCode_LWZ = 0x22,
    PCode_LWZX = 0x24,
    PCode_ADD = 0x3c,
    PCode_ADDI = 0x3f,
    PCode_XORI = 0x5a,
    PCode_RLWINM = 0x67,
    PCode_MFCR = 0x82,
    PCode_LI = 0x89,
    PCode_LIS = 0x8a,
    PCode_LFS = 0x8e,
    PCode_LFSX = 0x90,
    PCode_LFD = 0x92,
    PCode_LFDX = 0x94
};

extern unsigned char gOperandsDebug;   /* 0x00584244 */
extern signed char gOptimizationLevel; /* 0x005842e1 */
extern short gUsedVirtualRegistersFPR; /* 0x0058846c */
extern short gUsedVirtualRegistersGPR; /* 0x0058846e */

extern void Operands_Assert(const char* file, int line);  /* 0x00445780 */
extern void Operands_DebugType(int code);                 /* 0x0047cbd0 */
extern unsigned char Type_IsUnsigned(CompilerType* type); /* 0x0048f180 */
extern void Operands_ForceGPRPair(Operand* operand, CompilerType* type,
                                  short requested_register,
                                  short requested_second); /* 0x004a0680 */
extern void Operands_Normalize(Operand* operand);          /* 0x004a0fc0 */
extern void Operands_EmitMemoryInstruction(short opcode, short destination,
                                           short base, CompilerObject* object,
                                           int displacement); /* 0x004a1dd0 */
extern void PCodeUtilities_EmitAddress(short destination, short base,
                                       CompilerObject* object,
                                       short displacement);  /* 0x004a2060 */
extern void PCodeUtilities_EmitInstruction(int opcode, ...); /* 0x004a25d0 */
extern void Operands_PropagateFlags(unsigned int flags);     /* 0x0049cf70 */

static short Operands_AllocateGPR(short requested_register)
{
    if (requested_register != 0) {
        return requested_register;
    }
    return gUsedVirtualRegistersGPR++;
}

static short Operands_AllocateFPR(short requested_register)
{
    if (requested_register != 0) {
        return requested_register;
    }
    return gUsedVirtualRegistersFPR++;
}

static int Operands_RequiresGPRPair(const CompilerType* type)
{
    if ((type->kind == 1 || type->kind == 3) && type->size == 8) {
        return 1;
    }
    return gOperandsDebug != 0 && type->kind == 2 && type->size != 4;
}

static int Operands_IsValidWordType(const CompilerType* type)
{
    if (type->kind == 11) {
        return 1;
    }
    if (type->kind == 10 && type->size == 4) {
        return 1;
    }
    return gOperandsDebug != 0 && type->kind == 2 && type->size == 4;
}

static short Operands_SelectGPRLoad(CompilerType* type, int indexed,
                                    int assert_line)
{
    if (type->kind == 1 || type->kind == 3) {
        if (type->size == 1) {
            return indexed ? PCode_LBZX : PCode_LBZ;
        }
        if (type->size == 2) {
            if (Type_IsUnsigned(type)) {
                return indexed ? PCode_LHZX : PCode_LHZ;
            }
            return indexed ? PCode_LHAX : PCode_LHA;
        }
    } else if (!Operands_IsValidWordType(type)) {
        Operands_Assert("Operands.c", assert_line);
    }
    return indexed ? PCode_LWZX : PCode_LWZ;
}

static void Operands_DecodeCondition(short condition, short* bit,
                                     short* invert)
{
    *invert = 0;
    switch (condition) {
    case 0x13:
        *bit = 0;
        break;
    case 0x14:
        *bit = 1;
        break;
    case 0x15:
        *bit = 1;
        *invert = 1;
        break;
    case 0x16:
        *bit = 0;
        *invert = 1;
        break;
    case 0x17:
        *bit = 2;
        break;
    case 0x18:
        *bit = 2;
        *invert = 1;
        break;
    default:
        *bit = 0;
        Operands_Assert("Operands.c", 0x259);
        break;
    }
}

/* 0x004a0ba0; functionally equivalent; binary match unmeasured. */
void Operands_ForceGPR(Operand* operand, CompilerType* type,
                       short requested_register)
{
    int value;
    short bit;
    short destination;
    short high_register;
    short invert;
    short lower;
    short opcode;
    short upper;

    if (Operands_RequiresGPRPair(type)) {
        Operands_ForceGPRPair(operand, type, requested_register, 0);
        return;
    }

    Operands_Normalize(operand);
    switch (operand->kind) {
    case OperandKind_GPR:
    case OperandKind_GPRPair:
        return;
    case OperandKind_Address:
        destination = Operands_AllocateGPR(requested_register);
        PCodeUtilities_EmitAddress(destination, operand->reg, operand->object,
                                   operand->displacement);
        break;
    case OperandKind_GPRSum:
        destination = Operands_AllocateGPR(requested_register);
        PCodeUtilities_EmitInstruction(PCode_ADD, destination, operand->reg,
                                       operand->secondary_reg);
        break;
    case OperandKind_Immediate:
        destination = Operands_AllocateGPR(requested_register);
        value = operand->immediate;
        lower = (short) value;
        if (value == lower) {
            PCodeUtilities_EmitInstruction(PCode_LI, destination, value);
            break;
        }

        high_register = destination;
        if (gOptimizationLevel > 1 && lower != 0) {
            high_register = gUsedVirtualRegistersGPR++;
        }
        upper = (short) ((value >> 16) + ((value >> 15) & 1));
        PCodeUtilities_EmitInstruction(PCode_LIS, high_register, 0, upper);
        if (lower != 0) {
            PCodeUtilities_EmitInstruction(PCode_ADDI, destination,
                                           high_register, 0, lower);
        }
        break;
    case OperandKind_Condition:
        destination = Operands_AllocateGPR(requested_register);
        PCodeUtilities_EmitInstruction(PCode_MFCR, destination);
        Operands_DecodeCondition(operand->secondary_reg, &bit, &invert);
        PCodeUtilities_EmitInstruction(PCode_RLWINM, destination, destination,
                                       operand->reg * 4 + bit + 1, 31, 31);
        if (invert != 0) {
            PCodeUtilities_EmitInstruction(PCode_XORI, destination,
                                           destination, 1);
        }
        break;
    case OperandKind_DirectMemory:
        destination = Operands_AllocateGPR(requested_register);
        opcode = Operands_SelectGPRLoad(type, 0, 0x224);
        Operands_EmitMemoryInstruction(opcode, destination, operand->reg,
                                       operand->object, operand->displacement);
        Operands_PropagateFlags(operand->flags_0a);
        break;
    case OperandKind_IndexedMemory:
        destination = Operands_AllocateGPR(requested_register);
        opcode = Operands_SelectGPRLoad(type, 1, 0x23a);
        PCodeUtilities_EmitInstruction(opcode, destination, operand->reg,
                                       operand->secondary_reg);
        Operands_PropagateFlags(operand->flags_0a);
        break;
    default:
        Operands_Assert("Operands.c", 0x264);
        return;
    }

    operand->kind = OperandKind_GPR;
    operand->reg = destination;
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
