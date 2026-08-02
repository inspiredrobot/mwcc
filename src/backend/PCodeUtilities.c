/*
 * PCodeUtilities.c
 *
 * Working entry points:
 *   0x004a2660  PCodeUtilities_BuildInstructionV
 *
 * This is the common interpreter for the opcode descriptor's compact operand
 * format. The public variadic wrappers at 0x004a25d0 and 0x004a2620 pass their
 * first aligned argument slot here; their append-versus-return behavior is
 * recorded in the subsystem manifest but not modeled by this typed entry.
 */

#include "mwcc/PCode.h"

extern PCodeOpcodeDescriptor gPCodeOpcodeDescriptors[];        /* 0x005654b0 */
extern void* PCodeUtilities_Allocate(unsigned int size);       /* 0x00441fa0 */
extern void PCodeUtilities_Assert(const char* file, int line); /* 0x00445780 */
extern int PCodeUtilities_ObjectUsesDefinitionFlag(CompilerObject* object);
/* 0x0048ad10 */

static void PCodeUtilities_Require(int condition, int line)
{
    if (!condition) {
        PCodeUtilities_Assert("PCodeUtilities.c", line);
    }
}

static void PCodeUtilities_SetRegister(PCodeOperand* operand, int kind,
                                       int flags, int reg)
{
    operand->kind = (unsigned char) kind;
    operand->flags = (unsigned char) flags;
    operand->value.reg = (short) reg;
}

static void PCodeUtilities_SetValue(PCodeOperand* operand, int kind, int value,
                                    CompilerObject* object)
{
    operand->kind = (unsigned char) kind;
    operand->value.signed_value = value;
    operand->object = object;
}

static unsigned int PCodeUtilities_GetObjectFlags(CompilerObject* object)
{
    if (object->type->kind == 0x0b) {
        return object->type->flags_0a;
    }
    return object->flags_12;
}

static void PCodeUtilities_PropagateObjectFlags(PCodeInstruction* instruction,
                                                CompilerObject* object)
{
    unsigned int flags;

    if ((instruction->flags & PCodeInstruction_GPRResultMask) == 0) {
        return;
    }
    flags = PCodeUtilities_GetObjectFlags(object);
    if ((flags & 2) != 0) {
        instruction->flags |= PCodeInstruction_ObjectFlag2;
    }
    if ((flags & 1) != 0) {
        instruction->flags |= PCodeInstruction_ObjectFlag1;
    }
}

static PCodeBuildArgument*
PCodeUtilities_BuildMemoryOperand(PCodeInstruction* instruction,
                                  PCodeOperand* operand,
                                  PCodeBuildArgument* arguments, int strict)
{
    CompilerObject* object;

    object = arguments[0].object;
    if (object == 0) {
        PCodeUtilities_SetValue(operand, 4, arguments[1].signed_value, 0);
        if ((instruction->flags & PCodeInstruction_GPRResultMask) != 0) {
            instruction->flags |= PCodeInstruction_NullObjectMemory;
        }
        return arguments + 2;
    }

    PCodeUtilities_Require(object->object_tag == 5, strict ? 0xd4 : 0xa2);
    if (object->kind == 2) {
        PCodeUtilities_SetValue(operand, 4, arguments[1].signed_value, object);
        return arguments + 2;
    }

    PCodeUtilities_SetValue(operand, 5, arguments[1].signed_value, object);
    PCodeUtilities_PropagateObjectFlags(instruction, object);
    if (strict) {
        PCodeUtilities_Require(object->kind != 1, 0xea);
        operand->flags = 8;
    } else if ((instruction->flags & 0x24) != 0) {
        operand->flags = 4;
    } else if (object->kind == 1) {
        operand->flags = 1;
    } else if (PCodeUtilities_ObjectUsesDefinitionFlag(object)) {
        operand->flags = 2;
    } else {
        operand->flags = 6;
    }
    return arguments + 2;
}

static PCodeBuildArgument*
PCodeUtilities_BuildLabelOperand(PCodeOperand* operand,
                                 PCodeBuildArgument* arguments)
{
    CompilerObject* object;

    if (arguments[0].signed_value != 0) {
        PCodeUtilities_SetValue(operand, 6, arguments[0].signed_value, 0);
        return arguments + 1;
    }

    object = arguments[1].object;
    PCodeUtilities_Require(object->object_tag == 5, 0x97);
    PCodeUtilities_SetValue(operand, 5, 0, object);
    operand->flags = 4;
    return arguments + 2;
}

/* 0x004a2660; functionally equivalent; binary match unmeasured. */
PCodeInstruction*
PCodeUtilities_BuildInstructionV(short opcode, PCodeBuildArgument* arguments)
{
    PCodeOpcodeDescriptor* descriptor;
    PCodeInstruction* instruction;
    PCodeOperand* operand;
    const char* format;
    int dynamic_count;
    int operand_count;
    int access;
    int index;
    unsigned int size;

    descriptor = &gPCodeOpcodeDescriptors[opcode];
    format = descriptor->operand_format;
    dynamic_count = 0;
    operand_count = descriptor->operand_count;
    if (*format == '#') {
        dynamic_count = arguments->signed_value;
        operand_count += dynamic_count;
        arguments++;
        format++;
    }

    size = sizeof(PCodeInstruction) +
           (unsigned int) operand_count * sizeof(PCodeOperand);
    if ((descriptor->flags & PCodeInstruction_CloneExtraOperand) != 0) {
        size += sizeof(PCodeOperand);
    }
    instruction = PCodeUtilities_Allocate(size);
    instruction->opcode = opcode;
    instruction->flags = descriptor->flags;
    instruction->operand_count = (short) operand_count;
    operand = instruction->operands;

    while (*format != '\0') {
        if (*format == ',') {
            format++;
        }
        access = 1;
        if (*format == '=') {
            access = 2;
            format++;
        } else if (*format == '+') {
            access = 3;
            format++;
        }

        switch (*format) {
        case 'b':
            if (arguments->signed_value == 0) {
                access = 0;
            }
            PCodeUtilities_SetRegister(operand, 0, access,
                                       arguments->signed_value);
            arguments++;
            break;
        case 'r':
            PCodeUtilities_SetRegister(operand, 0, access,
                                       arguments->signed_value);
            arguments++;
            break;
        case 'f':
            PCodeUtilities_SetRegister(operand, 1, access,
                                       arguments->signed_value);
            arguments++;
            break;
        case 'S':
            PCodeUtilities_SetRegister(operand, 2, access,
                                       arguments->signed_value);
            arguments++;
            break;
        case 'C':
            PCodeUtilities_SetRegister(operand, 2, access, 1);
            break;
        case 'L':
            PCodeUtilities_SetRegister(operand, 2, access, 2);
            break;
        case 'X':
            PCodeUtilities_SetRegister(operand, 2, access, 0);
            break;
        case 'c':
            PCodeUtilities_SetRegister(operand, 3, access,
                                       arguments->signed_value);
            arguments++;
            break;
        case 'Y':
            for (index = 0; index < 8; index++) {
                PCodeUtilities_SetRegister(operand + index, 3, access, index);
            }
            operand += 8;
            format++;
            continue;
        case 'Z':
            PCodeUtilities_SetRegister(operand, 3, access, 0);
            break;
        case 'i':
            PCodeUtilities_SetValue(operand, 4, arguments->signed_value, 0);
            arguments++;
            break;
        case 'm':
            arguments = PCodeUtilities_BuildMemoryOperand(instruction, operand,
                                                          arguments, 0);
            break;
        case 'M':
            arguments = PCodeUtilities_BuildMemoryOperand(instruction, operand,
                                                          arguments, 1);
            break;
        case 'l':
            arguments = PCodeUtilities_BuildLabelOperand(operand, arguments);
            break;
        case 'v':
            PCodeUtilities_SetRegister(operand, 9, access,
                                       arguments->signed_value);
            arguments++;
            break;
        case 'V':
            for (index = 0; index < dynamic_count; index++) {
                PCodeUtilities_SetRegister(operand + index, 0, access,
                                           32 - dynamic_count + index);
            }
            operand += dynamic_count;
            format++;
            continue;
        case 'p':
            operand->kind = 10;
            break;
        default:
            PCodeUtilities_Require(0, 0x139);
            break;
        }
        operand++;
        format++;
    }
    return instruction;
}
