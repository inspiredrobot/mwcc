#include "mwcc/PCode.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

PCodeOpcodeDescriptor gPCodeOpcodeDescriptors[16];

static union Allocation {
    long double alignment;
    unsigned char bytes[1024];
} gAllocation;
static unsigned int gAllocationSize;
static int gObjectDefinitionFlag;

static void Check(int condition, const char* message)
{
    if (!condition) {
        fprintf(stderr, "PCode utilities test failed: %s\n", message);
        exit(1);
    }
}

void* PCodeUtilities_Allocate(unsigned int size)
{
    Check(size <= sizeof(gAllocation.bytes), "allocation capacity");
    gAllocationSize = size;
    memset(gAllocation.bytes, 0, sizeof(gAllocation.bytes));
    return gAllocation.bytes;
}

void PCodeUtilities_Assert(const char* file, int line)
{
    fprintf(stderr, "unexpected compiler assertion: %s:%d\n", file, line);
    exit(1);
}

int PCodeUtilities_ObjectUsesDefinitionFlag(CompilerObject* object)
{
    (void) object;
    return gObjectDefinitionFlag;
}

static void SetDescriptor(int opcode, const char* format, int count, int flags)
{
    PCodeOpcodeDescriptor* descriptor;

    descriptor = &gPCodeOpcodeDescriptors[opcode];
    memset(descriptor, 0, sizeof(*descriptor));
    descriptor->operand_format = format;
    descriptor->operand_count = (unsigned char) count;
    descriptor->flags = (unsigned short) flags;
}

static void TestBasicAndNullMemory(void)
{
    PCodeBuildArgument arguments[4];
    PCodeInstruction* instruction;

    memset(arguments, 0, sizeof(arguments));
    SetDescriptor(1, "=r,b,m,p", 4, PCodeInstruction_GPRResultMask);
    arguments[0].signed_value = 40;
    arguments[1].signed_value = 0;
    arguments[2].object = 0;
    arguments[3].signed_value = 12;
    instruction = PCodeUtilities_BuildInstructionV(1, arguments);

    Check(instruction->opcode == 1, "opcode");
    Check(instruction->operand_count == 4, "operand count");
    Check(instruction->operands[0].kind == 0 &&
              instruction->operands[0].flags == 2 &&
              instruction->operands[0].value.reg == 40,
          "register definition");
    Check(instruction->operands[1].kind == 0 &&
              instruction->operands[1].flags == 0,
          "r0 base has no use");
    Check(instruction->operands[2].kind == 4 &&
              instruction->operands[2].value.signed_value == 12 &&
              instruction->operands[2].object == 0,
          "null-object immediate memory");
    Check((instruction->flags & PCodeInstruction_NullObjectMemory) != 0,
          "null-object instruction flag");
    Check(instruction->operands[3].kind == 10, "marker operand");
}

static void TestObjectMemory(void)
{
    CompilerType type;
    CompilerObject object;
    PCodeBuildArgument arguments[2];
    PCodeInstruction* instruction;

    memset(&type, 0, sizeof(type));
    memset(&object, 0, sizeof(object));
    memset(arguments, 0, sizeof(arguments));
    type.kind = 1;
    object.object_tag = 5;
    object.type = &type;
    object.flags_12 = 3;
    arguments[0].object = &object;
    arguments[1].signed_value = 24;

    SetDescriptor(2, "m", 1, PCodeInstruction_GPRResultMask);
    object.kind = 1;
    instruction = PCodeUtilities_BuildInstructionV(2, arguments);
    Check(instruction->operands[0].kind == 5 &&
              instruction->operands[0].flags == 1,
          "kind-1 object memory");
    Check((instruction->flags & PCodeInstruction_ObjectFlag1) != 0 &&
              (instruction->flags & PCodeInstruction_ObjectFlag2) != 0,
          "object flags propagated");

    object.kind = 2;
    instruction = PCodeUtilities_BuildInstructionV(2, arguments);
    Check(instruction->operands[0].kind == 4 &&
              instruction->operands[0].object == &object,
          "kind-2 object becomes immediate");

    object.kind = 0;
    object.flags_12 = 0;
    gObjectDefinitionFlag = 1;
    instruction = PCodeUtilities_BuildInstructionV(2, arguments);
    Check(instruction->operands[0].flags == 2, "classified object definition");
    gObjectDefinitionFlag = 0;
    instruction = PCodeUtilities_BuildInstructionV(2, arguments);
    Check(instruction->operands[0].flags == 6, "other object memory flags");

    SetDescriptor(3, "M", 1, PCodeInstruction_GPRResultMask);
    instruction = PCodeUtilities_BuildInstructionV(3, arguments);
    Check(instruction->operands[0].flags == 8, "strict memory flags");
}

static void TestLabelsAndExpansions(void)
{
    CompilerObject object;
    PCodeBuildArgument arguments[2];
    PCodeInstruction* instruction;
    int index;

    memset(&object, 0, sizeof(object));
    memset(arguments, 0, sizeof(arguments));
    SetDescriptor(4, "l", 1, 0);
    arguments[0].signed_value = 0x1234;
    instruction = PCodeUtilities_BuildInstructionV(4, arguments);
    Check(instruction->operands[0].kind == 6 &&
              instruction->operands[0].value.signed_value == 0x1234,
          "direct label");

    object.object_tag = 5;
    arguments[0].signed_value = 0;
    arguments[1].object = &object;
    instruction = PCodeUtilities_BuildInstructionV(4, arguments);
    Check(instruction->operands[0].kind == 5 &&
              instruction->operands[0].flags == 4 &&
              instruction->operands[0].object == &object,
          "object label");

    SetDescriptor(5, "#V,Y", 8, PCodeInstruction_CloneExtraOperand);
    arguments[0].signed_value = 3;
    instruction = PCodeUtilities_BuildInstructionV(5, arguments);
    Check(instruction->operand_count == 11, "dynamic operand count");
    for (index = 0; index < 3; index++) {
        Check(instruction->operands[index].kind == 0 &&
                  instruction->operands[index].value.reg == 29 + index,
              "dynamic GPR expansion");
    }
    for (index = 0; index < 8; index++) {
        Check(instruction->operands[index + 3].kind == 3 &&
                  instruction->operands[index + 3].value.reg == index,
              "condition-register expansion");
    }
    Check(gAllocationSize ==
              sizeof(PCodeInstruction) + 12 * sizeof(PCodeOperand),
          "extra operand allocation");
}

int main(void)
{
    TestBasicAndNullMemory();
    TestObjectMemory();
    TestLabelsAndExpansions();
    puts("PCode utilities model tests passed");
    return 0;
}
