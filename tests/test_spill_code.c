#include "mwcc/SpillCode.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

unsigned char gCOptimizerDumpEnabled;
unsigned char gUniformSpillBlockWeight;
InterferenceNode** gInterferenceGraph;
PCodeBlock* gPCodeBlocks;

static InterferenceNode gNodes[40];
static InterferenceNode* gNodePointers[40];
static int gPipeline[6];
static int gPipelineLength;
static int gLastClass;
static int gLastCount;
static const char* gLastFormat;

static void Check(int condition, const char* message)
{
    if (!condition) {
        fprintf(stderr, "spill-code test failed: %s\n", message);
        exit(1);
    }
}

static void RecordStage(int stage, int reg_class, int register_count)
{
    Check(gPipelineLength < 6, "pipeline stage count");
    gPipeline[gPipelineLength++] = stage;
    gLastClass = reg_class;
    gLastCount = register_count;
}

void SpillCode_005301b0(PCodeFunction* function, int reg_class,
                        int register_count)
{
    (void) function;
    RecordStage(1, reg_class, register_count);
}

void SpillCode_00530a80(int reg_class, int register_count)
{
    RecordStage(2, reg_class, register_count);
}

void SpillCode_00531290(int reg_class, int register_count)
{
    RecordStage(3, reg_class, register_count);
}

void SpillCode_00530e00(int reg_class, int register_count)
{
    RecordStage(4, reg_class, register_count);
}

void SpillCode_00530c00(int register_count)
{
    RecordStage(5, gLastClass, register_count);
}

void SpillCode_DumpInterference(const char* format, int register_count)
{
    gLastFormat = format;
    RecordStage(6, gLastClass, register_count);
}

static void ResetState(void)
{
    int reg;

    memset(gNodes, 0, sizeof(gNodes));
    for (reg = 0; reg < 40; reg++) {
        gNodePointers[reg] = &gNodes[reg];
    }
    gInterferenceGraph = gNodePointers;
    gPCodeBlocks = 0;
    gCOptimizerDumpEnabled = 0;
    gUniformSpillBlockWeight = 0;
    memset(gPipeline, 0, sizeof(gPipeline));
    gPipelineLength = 0;
    gLastClass = -1;
    gLastCount = 0;
    gLastFormat = 0;
}

static void TestInterferencePipeline(void)
{
    int expected[] = {1, 2, 3, 6, 4, 5};
    int index;

    ResetState();
    gCOptimizerDumpEnabled = 1;
    SpillCode_BuildInterference((PCodeFunction*) 1, 9, 45);
    Check(gPipelineLength == 6, "interference pipeline length");
    for (index = 0; index < 6; index++) {
        Check(gPipeline[index] == expected[index], "interference stage order");
    }
    Check(gLastClass == 9 && gLastCount == 45, "interference arguments");
    Check(strcmp(gLastFormat, " vr%ld") == 0, "vector dump format");
}

static void TestSpillCosts(void)
{
    typedef struct TestInstruction {
        PCodeInstruction instruction;
        PCodeOperand extra_operands[2];
    } TestInstruction;

    TestInstruction first_instruction;
    TestInstruction second_instruction;
    PCodeBlock first_block;
    PCodeBlock second_block;

    ResetState();
    memset(&first_instruction, 0, sizeof(first_instruction));
    memset(&second_instruction, 0, sizeof(second_instruction));
    memset(&first_block, 0, sizeof(first_block));
    memset(&second_block, 0, sizeof(second_block));

    first_instruction.instruction.operand_count = 3;
    first_instruction.instruction.operands[0].kind = 0;
    first_instruction.instruction.operands[0].flags = 1;
    first_instruction.instruction.operands[0].reg = 32;
    first_instruction.extra_operands[0].kind = 0;
    first_instruction.extra_operands[0].flags = 2;
    first_instruction.extra_operands[0].reg = 33;
    first_instruction.extra_operands[1].kind = 0;
    first_instruction.extra_operands[1].flags = 3;
    first_instruction.extra_operands[1].reg = 34;

    second_instruction.instruction.operand_count = 1;
    second_instruction.instruction.operands[0].kind = 0;
    second_instruction.instruction.operands[0].flags = 1;
    second_instruction.instruction.operands[0].reg = 32;

    first_block.instructions = &first_instruction.instruction;
    first_block.execution_weight = 3;
    first_block.next = &second_block;
    second_block.instructions = &second_instruction.instruction;
    second_block.execution_weight = 5;
    gPCodeBlocks = &first_block;

    SpillCode_ComputeSpillCosts(0);
    Check(gNodes[32].spill_cost == 16, "weighted read cost");
    Check(gNodes[33].spill_cost == 3, "weighted write cost");
    Check(gNodes[34].spill_cost == 9, "read-write cost");

    gNodes[32].spill_cost = 0;
    gNodes[33].spill_cost = 0;
    gNodes[34].spill_cost = 0;
    gUniformSpillBlockWeight = 1;
    SpillCode_ComputeSpillCosts(0);
    Check(gNodes[32].spill_cost == 4, "uniform read cost");
    Check(gNodes[33].spill_cost == 1, "uniform write cost");
    Check(gNodes[34].spill_cost == 3, "uniform read-write cost");
}

int main(void)
{
    TestInterferencePipeline();
    TestSpillCosts();
    puts("spill-code model tests passed");
    return 0;
}
