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
PCodeBlock** gPCodeBlockOrder;
int gPCodeBlockCount;
PCodeBlock* gReturnBlock;
PCodeBlock* gCurrentBlock;
unsigned int* gInterferenceBits;
short* gCoalescedRegisters;
short gGPRCoalesceFirst;
short gGPRCoalesceLast;
short gFPRCoalesceFirst;
short gFPRCoalesceLast;
short gVRCoalesceFirst;
short gVRCoalesceLast;
unsigned char gUseGPRForType2Return;

static InterferenceNode gNodes[40];
static InterferenceNode* gNodePointers[40];
static int gPipeline[2];
static int gPipelineLength;
static int gLastClass;
static int gLastCount;
static const char* gLastFormat;
static int gRemovedInstructions;
static int gRequiresMemoryReturn;

static int BitIsSet(const unsigned int* bits, int bit);

static void Check(int condition, const char* message)
{
    if (!condition) {
        fprintf(stderr, "spill-code test failed: %s\n", message);
        exit(1);
    }
}

static void RecordStage(int stage, int reg_class, int register_count)
{
    Check(gPipelineLength < 2, "pipeline stage count");
    gPipeline[gPipelineLength++] = stage;
    gLastClass = reg_class;
    gLastCount = register_count;
}

void SpillCode_BuildBlockOrder(void)
{
    Check(gPipelineLength < 2, "pipeline stage count");
    gPipeline[gPipelineLength++] = 1;
}

int Type_RequiresMemoryReturn(CompilerType* type)
{
    (void) type;
    return gRequiresMemoryReturn;
}

void SpillCode_DumpInterference(const char* format, int register_count)
{
    gLastFormat = format;
    if (strcmp(format, " r%ld") == 0) {
        RecordStage(2, 0, register_count);
    } else if (strcmp(format, " f%ld") == 0) {
        RecordStage(2, 1, register_count);
    } else {
        RecordStage(2, 9, register_count);
    }
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
    gPCodeBlockOrder = 0;
    gPCodeBlockCount = 0;
    gReturnBlock = 0;
    gCurrentBlock = 0;
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
    gGPRCoalesceFirst = 32;
    gGPRCoalesceLast = 39;
    gFPRCoalesceFirst = 32;
    gFPRCoalesceLast = 39;
    gVRCoalesceFirst = 32;
    gVRCoalesceLast = 39;
    gUseGPRForType2Return = 0;
    gRequiresMemoryReturn = 0;
}

static void TestInterferencePipeline(void)
{
    CompilerType result_type;
    PCodeFunctionSignature signature;
    PCodeFunction function;
    int expected[] = {1, 2};
    int index;

    ResetState();
    memset(&result_type, 0, sizeof(result_type));
    memset(&signature, 0, sizeof(signature));
    memset(&function, 0, sizeof(function));
    signature.result_type = &result_type;
    function.signature = &signature;
    gCOptimizerDumpEnabled = 1;
    SpillCode_BuildInterference(&function, 9, 40);
    Check(gPipelineLength == 2, "external interference pipeline length");
    for (index = 0; index < 2; index++) {
        Check(gPipeline[index] == expected[index], "interference stage order");
    }
    Check(gLastClass == 9 && gLastCount == 40, "interference arguments");
    Check(strcmp(gLastFormat, " vr%ld") == 0, "vector dump format");
}

static void TestLivenessInitialization(void)
{
    CompilerType result_type;
    PCodeFunctionSignature signature;
    PCodeFunction function;
    PCodeBlock block;
    PCodeBlock* order[1];

    ResetState();
    memset(&result_type, 0, sizeof(result_type));
    memset(&signature, 0, sizeof(signature));
    memset(&function, 0, sizeof(function));
    memset(&block, 0, sizeof(block));

    result_type.kind = 1;
    result_type.size = 8;
    signature.result_type = &result_type;
    function.signature = &signature;
    block.index = 0;
    order[0] = &block;
    gPCodeBlocks = &block;
    gPCodeBlockOrder = order;
    gPCodeBlockCount = 1;
    gReturnBlock = &block;
    gCurrentBlock = &block;

    SpillCode_InitializeLiveness(&function, 0, 40);
    Check(gPCodeBlockLiveness != 0, "liveness records allocated");
    Check(gPCodeBlockLiveness[0].use != 0 && gPCodeBlockLiveness[0].def != 0 &&
              gPCodeBlockLiveness[0].live_in != 0 &&
              gPCodeBlockLiveness[0].live_out != 0,
          "four liveness sets allocated");
    Check(BitIsSet(gPCodeBlockLiveness[0].use, 3),
          "primary GPR return register seeded");
    Check(BitIsSet(gPCodeBlockLiveness[0].use, 4),
          "secondary GPR return register seeded");
    Check(BitIsSet(gPCodeBlockLiveness[0].live_in, 3) &&
              BitIsSet(gPCodeBlockLiveness[0].live_in, 4),
          "return registers propagated into live-in");
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

static int BitIsSet(const unsigned int* bits, int bit)
{
    return (bits[bit >> 5] & (1U << (bit & 31))) != 0;
}

static void TestLocalLiveness(void)
{
    typedef struct TestInstruction {
        PCodeInstruction instruction;
        PCodeOperand second_operand;
    } TestInstruction;

    TestInstruction instruction;
    PCodeBlock block;
    PCodeBlockLiveness liveness;
    unsigned int use[2];
    unsigned int def[2];

    ResetState();
    memset(&instruction, 0, sizeof(instruction));
    memset(&block, 0, sizeof(block));
    memset(&liveness, 0, sizeof(liveness));
    memset(use, 0, sizeof(use));
    memset(def, 0, sizeof(def));

    liveness.use = use;
    liveness.def = def;
    gPCodeBlockLiveness = &liveness;
    block.instructions = &instruction.instruction;
    gPCodeBlocks = &block;
    instruction.instruction.operand_count = 2;
    instruction.instruction.operands[0].kind = 0;
    instruction.instruction.operands[0].flags = PCodeOperand_Use;
    instruction.instruction.operands[0].reg = 33;
    instruction.second_operand.kind = 0;
    instruction.second_operand.flags = PCodeOperand_Definition;
    instruction.second_operand.reg = 34;

    SpillCode_BuildLocalLiveness(0);
    Check(BitIsSet(use, 33), "upward-exposed use recorded");
    Check(BitIsSet(def, 34), "definition recorded");
    Check(!BitIsSet(use, 34), "definition is not a use");
    Check(!BitIsSet(def, 33), "use is not a definition");
}

static void TestLivenessFixedPoint(void)
{
    PCodeBlock first;
    PCodeBlock second;
    PCodeBlockLink successor;
    PCodeBlock* order[2];
    PCodeBlockLiveness liveness[2];
    unsigned int sets[2][4][2];
    int block_index;

    ResetState();
    memset(&first, 0, sizeof(first));
    memset(&second, 0, sizeof(second));
    memset(&successor, 0, sizeof(successor));
    memset(liveness, 0, sizeof(liveness));
    memset(sets, 0, sizeof(sets));

    for (block_index = 0; block_index < 2; block_index++) {
        liveness[block_index].use = sets[block_index][0];
        liveness[block_index].def = sets[block_index][1];
        liveness[block_index].live_in = sets[block_index][2];
        liveness[block_index].live_out = sets[block_index][3];
    }
    first.index = 0;
    first.successors = &successor;
    second.index = 1;
    successor.block = &second;
    order[0] = &first;
    order[1] = &second;
    gPCodeBlockLiveness = liveness;
    gPCodeBlockOrder = order;
    gPCodeBlockCount = 2;
    sets[0][1][1] = 1U << 1;
    sets[1][0][1] = 1U << 1;

    SpillCode_SolveLiveness(64);
    Check(BitIsSet(liveness[1].live_in, 33), "successor use reaches live-in");
    Check(BitIsSet(liveness[0].live_out, 33),
          "successor live-in reaches predecessor live-out");
    Check(!BitIsSet(liveness[0].live_in, 33),
          "predecessor definition kills live-in");
}

static int HasInterference(const unsigned int* bits, int first, int second)
{
    unsigned int index;
    int larger;
    int smaller;

    larger = first > second ? first : second;
    smaller = first > second ? second : first;
    index = (unsigned int) ((larger * larger) / 2 + smaller);
    return (bits[index >> 5] & (1U << (index & 31))) != 0;
}

static void TestInterferenceConstruction(void)
{
    typedef struct TestInstruction {
        PCodeInstruction instruction;
        PCodeOperand second_operand;
    } TestInstruction;

    TestInstruction definition;
    PCodeBlock block;
    PCodeBlockLiveness liveness;
    unsigned int live_out[2];

    ResetState();
    memset(&definition, 0, sizeof(definition));
    memset(&block, 0, sizeof(block));
    memset(&liveness, 0, sizeof(liveness));
    memset(live_out, 0, sizeof(live_out));

    live_out[1] = 1U << 2;
    liveness.live_out = live_out;
    gPCodeBlockLiveness = &liveness;
    block.reverse_instructions = &definition.instruction;
    gPCodeBlocks = &block;
    definition.instruction.operand_count = 1;
    definition.instruction.operands[0].kind = 0;
    definition.instruction.operands[0].flags = PCodeOperand_Definition;
    definition.instruction.operands[0].reg = 33;

    SpillCode_ConstructInterference(0, 40);
    Check(HasInterference(gInterferenceBits, 0, 31),
          "physical registers form a clique");
    Check(HasInterference(gInterferenceBits, 33, 34),
          "definition interferes with live-out register");

    ResetState();
    memset(&definition, 0, sizeof(definition));
    memset(&block, 0, sizeof(block));
    memset(&liveness, 0, sizeof(liveness));
    memset(live_out, 0, sizeof(live_out));

    live_out[1] = 1U << 2;
    liveness.live_out = live_out;
    gPCodeBlockLiveness = &liveness;
    block.reverse_instructions = &definition.instruction;
    gPCodeBlocks = &block;
    definition.instruction.flags = PCodeInstruction_CopySourceExclusion;
    definition.instruction.operand_count = 2;
    definition.instruction.operands[0].kind = 0;
    definition.instruction.operands[0].flags = PCodeOperand_Definition;
    definition.instruction.operands[0].reg = 33;
    definition.second_operand.kind = 0;
    definition.second_operand.flags = PCodeOperand_Use;
    definition.second_operand.reg = 34;

    SpillCode_ConstructInterference(0, 40);
    Check(!HasInterference(gInterferenceBits, 33, 34),
          "copy source excluded from destination interference");

    definition.instruction.flags = PCodeInstruction_GPRResultMask;
    definition.instruction.operands[1].reg = 39;
    SpillCode_ConstructInterference(0, 40);
    Check((gInterferenceBits[((39 * 39) / 2) >> 5] &
           (1U << (((39 * 39) / 2) & 31))) != 0,
          "GPR constraint uses matrix diagonal encoding");
}

static void TestCopyCoalescing(void)
{
    typedef struct TestInstruction {
        PCodeInstruction instruction;
        PCodeOperand second_operand;
    } TestInstruction;

    TestInstruction copy;
    PCodeBlock block;
    unsigned int bits[32];

    ResetState();
    memset(&copy, 0, sizeof(copy));
    memset(&block, 0, sizeof(block));
    memset(bits, 0, sizeof(bits));

    SetInterference(bits, 33, 34);
    gInterferenceBits = bits;
    copy.instruction.opcode = 0x8b;
    copy.instruction.operand_count = 2;
    copy.instruction.operands[0].kind = 0;
    copy.instruction.operands[0].reg = 32;
    copy.second_operand.kind = 0;
    copy.second_operand.reg = 33;
    block.instructions = &copy.instruction;
    gPCodeBlocks = &block;

    SpillCode_CoalesceCopies(0, 40);
    Check(gCoalescedRegisters[33] == 32, "lower copy register becomes root");
    Check(copy.instruction.operands[0].reg == 32,
          "copy destination canonicalized");
    Check(copy.second_operand.reg == 32, "copy source canonicalized");
    Check(gRemovedInstructions == 1, "coalesced copy removed");
    Check(HasInterference(bits, 32, 34),
          "child interference transferred to root");
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
    TestLivenessInitialization();
    TestLocalLiveness();
    TestLivenessFixedPoint();
    TestLastUseMarkers();
    TestInterferenceConstruction();
    TestCopyCoalescing();
    TestGraphMaterialization();
    TestSpillCosts();
    puts("spill-code model tests passed");
    return 0;
}
