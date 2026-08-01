#include "mwcc/SpillCode.h"

#include "mwcc/Coloring.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

unsigned char gCOptimizerDumpEnabled;
unsigned char gUniformSpillBlockWeight;
InterferenceNode** gInterferenceGraph;
PCodeBlock* gPCodeBlocks;
PCodeBlockLiveness* gPCodeBlockLiveness;
unsigned int* gInterferenceBits;
short* gCoalescedRegisters;

static InterferenceNode gNodes[40];
static InterferenceNode* gNodePointers[40];
static int gPipeline[4];
static int gPipelineLength;
static int gLastClass;
static int gLastCount;
static const char* gLastFormat;
static int gRemovedInstructions;

static void Check(int condition, const char* message)
{
    if (!condition) {
        fprintf(stderr, "spill-code test failed: %s\n", message);
        exit(1);
    }
}

static void RecordStage(int stage, int reg_class, int register_count)
{
    Check(gPipelineLength < 4, "pipeline stage count");
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

void SpillCode_00531290(int reg_class, int register_count)
{
    RecordStage(2, reg_class, register_count);
}

void SpillCode_00530e00(int reg_class, int register_count)
{
    RecordStage(4, reg_class, register_count);
}

void SpillCode_DumpInterference(const char* format, int register_count)
{
    gLastFormat = format;
    RecordStage(3, gLastClass, register_count);
}

void* SpillCode_Allocate(unsigned int size)
{
    void* result;

    result = calloc(1, size == 0 ? 1 : size);
    Check(result != 0, "model allocation");
    return result;
}

int SpillCode_HandleSpecialInstruction(PCodeInstruction* instruction,
                                       int reg_class, unsigned int* live)
{
    (void) instruction;
    (void) reg_class;
    (void) live;
    return 0;
}

void SpillCode_CopyLiveSet(unsigned int* destination, const void* source,
                           int register_count)
{
    unsigned int word_count;

    word_count = (unsigned int) ((register_count + 31) >> 5);
    memcpy(destination, source, word_count * sizeof(*destination));
}

void PCode_RemoveRedundantInstruction(PCodeInstruction* instruction)
{
    (void) instruction;
    gRemovedInstructions++;
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
    gPCodeBlockLiveness = 0;
    gInterferenceBits = 0;
    gCoalescedRegisters = 0;
    gCOptimizerDumpEnabled = 0;
    gUniformSpillBlockWeight = 0;
    memset(gPipeline, 0, sizeof(gPipeline));
    gPipelineLength = 0;
    gLastClass = -1;
    gLastCount = 0;
    gLastFormat = 0;
    gRemovedInstructions = 0;
}

static void TestInterferencePipeline(void)
{
    int expected[] = {1, 2, 3, 4};
    int index;
    short coalesced;
    unsigned int bits;

    ResetState();
    coalesced = 0;
    bits = 0;
    gCoalescedRegisters = &coalesced;
    gInterferenceBits = &bits;
    gCOptimizerDumpEnabled = 1;
    SpillCode_BuildInterference((PCodeFunction*) 1, 9, 0);
    Check(gPipelineLength == 4, "external interference pipeline length");
    for (index = 0; index < 4; index++) {
        Check(gPipeline[index] == expected[index], "interference stage order");
    }
    Check(gLastClass == 9 && gLastCount == 0, "interference arguments");
    Check(strcmp(gLastFormat, " vr%ld") == 0, "vector dump format");
}

static void TestLastUseMarkers(void)
{
    typedef struct TestInstruction {
        PCodeInstruction instruction;
        PCodeOperand second_operand;
    } TestInstruction;

    TestInstruction early;
    PCodeInstruction late;
    PCodeBlock block;
    PCodeBlockLiveness liveness;
    unsigned int live_out[2];

    ResetState();
    memset(&early, 0, sizeof(early));
    memset(&late, 0, sizeof(late));
    memset(&block, 0, sizeof(block));
    memset(&liveness, 0, sizeof(liveness));
    memset(live_out, 0, sizeof(live_out));

    live_out[1] = 1U << 1;
    liveness.live_out = live_out;
    gPCodeBlockLiveness = &liveness;
    block.reverse_instructions = &late;
    gPCodeBlocks = &block;

    late.previous = &early.instruction;
    late.operand_count = 1;
    late.operands[0].kind = 0;
    late.operands[0].flags = PCodeOperand_Definition;
    late.operands[0].reg = 32;

    early.instruction.operand_count = 2;
    early.instruction.operands[0].kind = 0;
    early.instruction.operands[0].flags = PCodeOperand_Use;
    early.instruction.operands[0].reg = 32;
    early.second_operand.kind = 0;
    early.second_operand.flags = PCodeOperand_Use;
    early.second_operand.reg = 33;

    SpillCode_MarkLastUses(0, 64);
    Check((early.instruction.operands[0].flags & PCodeOperand_LastUse) != 0,
          "newly-live use marker");
    Check((early.second_operand.flags & PCodeOperand_LastUse) == 0,
          "live-out use has no marker");
    Check(gRemovedInstructions == 0, "ordinary liveness instruction");
}

static void SetInterference(unsigned int* bits, int first, int second)
{
    unsigned int index;
    int larger;
    int smaller;

    larger = first > second ? first : second;
    smaller = first > second ? second : first;
    index = (unsigned int) ((larger * larger) / 2 + smaller);
    bits[index >> 5] |= 1U << (index & 31);
}

static void TestGraphMaterialization(void)
{
    unsigned int bits[2];
    short coalesced[4];

    ResetState();
    memset(bits, 0, sizeof(bits));
    coalesced[0] = 0;
    coalesced[1] = 1;
    coalesced[2] = 1;
    coalesced[3] = 3;
    SetInterference(bits, 0, 2);
    SetInterference(bits, 1, 2);
    gInterferenceBits = bits;
    gCoalescedRegisters = coalesced;

    SpillCode_MaterializeGraph(4);
    Check(gInterferenceGraph[0]->neighbor_count == 1, "single graph neighbor");
    Check(gInterferenceGraph[0]->neighbors[0] == 2, "graph neighbor index");
    Check(gInterferenceGraph[2]->degree == 2, "graph degree");
    Check((gInterferenceGraph[2]->flags & Interference_Coalesced) != 0,
          "coalesced-node flag");
    Check(gInterferenceGraph[2]->physical_register == 1,
          "coalescing root link");
    Check((gInterferenceGraph[1]->flags & Interference_CoalesceTarget) != 0,
          "coalescing-root flag");
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
    TestLastUseMarkers();
    TestGraphMaterialization();
    TestSpillCosts();
    puts("spill-code model tests passed");
    return 0;
}
