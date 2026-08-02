#include "mwcc/COpt.h"

#include "mwcc/backend_types.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

CodeMotionNode* gCodeMotionTree_0058763c;
PCodeBlock* gPCodeBlocks;
int gPCodeBlockCount;
void* gCodeMotionAllocationList_005870fc;
void* gCodeMotionObjectTree_005880ac;
int gCodeMotionDefinitionCount_00587ebc;
int gCodeMotionUseCount_00587e38;
void* gCodeMotionBlockState_00587fe4;
int gCodeMotionCounter_005880b8;
int gCodeMotionChanged;

static unsigned int gAllocationPool[256];
static int gAllocationOffset;
static int gAllocationSizes[32];
static int gAllocationCount;
static int gStages[32];
static int gStageCount;
static CompilerObject* gRecordedObject;
static CodeMotionNode* gRecordedNode;

static void Check(int condition, const char* message)
{
    if (!condition) {
        fprintf(stderr, "code-motion test failed: %s\n", message);
        exit(1);
    }
}

static void RecordStage(int stage)
{
    Check(gStageCount < 32, "stage capacity");
    gStages[gStageCount++] = stage;
}

void* CodeMotion_Allocate(unsigned int size)
{
    unsigned int word_count;
    void* result;

    Check(gAllocationCount < 32, "allocation capacity");
    gAllocationSizes[gAllocationCount++] = (int) size;
    word_count = (size + sizeof(unsigned int) - 1) / sizeof(unsigned int);
    result = &gAllocationPool[gAllocationOffset];
    gAllocationOffset += (int) word_count;
    Check(gAllocationOffset <= 256, "allocation pool");
    return result;
}

void CodeMotion_FreeIteration(void)
{
    RecordStage(9);
}

void SpillCode_BuildBlockOrder(void)
{
    RecordStage(6);
}

void COpt_00521a30(CodeMotionNode* node)
{
    gRecordedNode = node;
    RecordStage(1);
}

void COpt_005237f0(void)
{
    RecordStage(8);
}

void COpt_00523920(void)
{
    RecordStage(7);
}

void COpt_00523a50(void)
{
    RecordStage(5);
}

void COpt_005240b0(int mode)
{
    Check(mode == 1, "second setup mode");
    RecordStage(4);
}

void COpt_005246d0(int mode)
{
    Check(mode == 1, "first setup mode");
    RecordStage(3);
}

void COpt_00524b20(CompilerObject* object)
{
    gRecordedObject = object;
    RecordStage(2);
}

void COpt_00524c10(CodeMotionNode* node)
{
    gRecordedNode = node;
    RecordStage(10);
}

void COpt_00525070(CodeMotionNode* node)
{
    Check(node == gRecordedNode, "shared tree root");
    RecordStage(11);
}

static void ResetState(void)
{
    memset(gAllocationPool, 0, sizeof(gAllocationPool));
    memset(gAllocationSizes, 0, sizeof(gAllocationSizes));
    memset(gStages, 0, sizeof(gStages));
    gAllocationOffset = 0;
    gAllocationCount = 0;
    gStageCount = 0;
    gRecordedObject = 0;
    gRecordedNode = 0;
    gCodeMotionTree_0058763c = 0;
    gPCodeBlocks = 0;
    gPCodeBlockCount = 0;
    gCodeMotionAllocationList_005870fc = 0;
    gCodeMotionObjectTree_005880ac = 0;
    gCodeMotionDefinitionCount_00587ebc = 0;
    gCodeMotionUseCount_00587e38 = 0;
    gCodeMotionBlockState_00587fe4 = 0;
    gCodeMotionCounter_005880b8 = 0;
    gCodeMotionChanged = 0;
}

static void TestGuardedTreePass(void)
{
    int root;

    ResetState();
    COpt_00521a10();
    Check(gStageCount == 0, "null tree skipped");

    root = 0;
    gCodeMotionTree_0058763c = (CodeMotionNode*) &root;
    COpt_00521a10();
    Check(gStageCount == 1 && gStages[0] == 1, "tree pass called");
    Check(gRecordedNode == gCodeMotionTree_0058763c, "tree pass root");
}

static void TestSetup(void)
{
    typedef struct TestInstruction {
        PCodeInstruction instruction;
        PCodeOperand extra_operands[2];
    } TestInstruction;

    PCodeBlock block;
    TestInstruction instruction;
    CompilerObject object;
    int expected_stages[] = {2, 3, 4, 5, 6, 7, 8};
    int index;

    ResetState();
    memset(&block, 0, sizeof(block));
    memset(&instruction, 0, sizeof(instruction));
    memset(&object, 0, sizeof(object));
    instruction.instruction.flags = PCodeInstruction_GPRResultMask;
    instruction.instruction.operand_count = 3;
    instruction.instruction.operands[2].object = &object;
    block.instructions = &instruction.instruction;
    gPCodeBlocks = &block;
    gPCodeBlockCount = 2;
    gCodeMotionDefinitionCount_00587ebc = 33;
    gCodeMotionUseCount_00587e38 = 65;
    gCodeMotionAllocationList_005870fc = &object;
    gCodeMotionObjectTree_005880ac = &object;

    COpt_SetLoopCodeMotionMode(1);

    Check(gRecordedObject == &object, "candidate object collected");
    Check(gCodeMotionAllocationList_005870fc == 0, "allocation list reset");
    Check(gCodeMotionObjectTree_005880ac == 0, "object tree reset");
    Check(gAllocationCount == 17, "block state and eight sets per block");
    Check(gAllocationSizes[0] == 2 * 8 * (int) sizeof(void*),
          "two block-state records");
    for (index = 1; index < 17; index++) {
        if (((index - 1) & 7) < 4) {
            Check(gAllocationSizes[index] == 8, "definition-set size");
        } else {
            Check(gAllocationSizes[index] == 12, "use-set size");
        }
    }
    Check(gStageCount == 7, "setup stage count");
    for (index = 0; index < 7; index++) {
        Check(gStages[index] == expected_stages[index], "setup stage order");
    }
}

static void TestCoordinator(void)
{
    int root;

    ResetState();
    root = 0;
    gCodeMotionTree_0058763c = (CodeMotionNode*) &root;
    gCodeMotionCounter_005880b8 = 12;
    gCodeMotionChanged = 1;

    COpt_00524bd0();

    Check(gCodeMotionCounter_005880b8 == 0, "coordinator counter reset");
    Check(gCodeMotionChanged == 0, "change flag reset");
    Check(gStageCount == 3, "coordinator stage count");
    Check(gStages[0] == 10 && gStages[1] == 11 && gStages[2] == 9,
          "coordinator stage order");
}

int main(void)
{
    TestGuardedTreePass();
    TestSetup();
    TestCoordinator();
    puts("code-motion model tests passed");
    return 0;
}
