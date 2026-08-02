#include "mwcc/PCode.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned char gAllocation[256];
static unsigned int gAllocationSize;

static void Check(int condition, const char* message)
{
    if (!condition) {
        fprintf(stderr, "PCode test failed: %s\n", message);
        exit(1);
    }
}

void* PCode_Allocate(unsigned int size)
{
    Check(size <= sizeof(gAllocation), "allocation capacity");
    gAllocationSize = size;
    memset(gAllocation, 0, sizeof(gAllocation));
    return gAllocation;
}

static void TestClone(unsigned int flags, unsigned int extra_size)
{
    struct TestInstruction {
        PCodeInstruction instruction;
        PCodeOperand extra_operands[2];
    } source;
    PCodeInstruction* clone;
    unsigned int expected_size;

    memset(&source, 0, sizeof(source));
    source.instruction.next = (PCodeInstruction*) 1;
    source.instruction.previous = (PCodeInstruction*) 2;
    source.instruction.block = (PCodeBlock*) 3;
    source.instruction.opcode = 0x3f;
    source.instruction.flags = flags;
    source.instruction.operand_count = 3;
    source.instruction.operands[0].kind = 0;
    source.instruction.operands[1].kind = 4;
    source.instruction.operands[2].kind = 10;

    clone = PCode_CloneInstruction(&source.instruction);
    expected_size =
        sizeof(PCodeInstruction) + 3 * sizeof(PCodeOperand) + extra_size;
    Check(gAllocationSize == expected_size, "allocation size");
    Check(clone->next == 0 && clone->previous == 0 && clone->block == 0,
          "list and block fields remain unowned");
    Check(clone->opcode == source.instruction.opcode, "opcode copy");
    Check(clone->flags == source.instruction.flags, "flags copy");
    Check(clone->operand_count == source.instruction.operand_count,
          "operand count copy");
    Check(memcmp(clone->operands, source.instruction.operands,
                 3 * sizeof(PCodeOperand)) == 0,
          "operand copy");
}

int main(void)
{
    TestClone(0, 0);
    TestClone(PCodeInstruction_CloneExtraOperand, sizeof(PCodeOperand));
    TestClone(PCodeInstruction_CloneExtraOperand |
                  PCodeInstruction_CloneExtraOperandExcluded,
              0);
    puts("PCode model tests passed");
    return 0;
}
