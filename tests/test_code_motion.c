#include "mwcc/COpt.h"

#include "mwcc/backend_types.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

CodeMotionNode* gCodeMotionTree_0058763c;
PCodeBlock* gPCodeBlocks;
int gPCodeBlockCount;
CodeMotionObjectNode* gCodeMotionAllocationList_005870fc;
CodeMotionObjectNode* gCodeMotionObjectTree_005880ac;
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
static CodeMotionNode* gRecordedNodes[32];
static int gRecordedNodeCount;

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

unsigned char COpt_0048ad10(CompilerObject* object)
{
    return object->unknown_01;
}

static void RecordNode(int stage, CodeMotionNode* node)
{
    Check(gRecordedNodeCount < 32, "node capacity");
    gRecordedNodes[gRecordedNodeCount++] = node;
    RecordStage(stage);
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

void COpt_00524d90(CodeMotionNode* node)
{
    RecordNode(10, node);
}

void COpt_00525200(CodeMotionNode* node)
{
    RecordNode(11, node);
}

static void ResetState(void)
{
    memset(gAllocationPool, 0, sizeof(gAllocationPool));
    memset(gAllocationSizes, 0, sizeof(gAllocationSizes));
    memset(gStages, 0, sizeof(gStages));
    gAllocationOffset = 0;
    gAllocationCount = 0;
    gStageCount = 0;
    memset(gRecordedNodes, 0, sizeof(gRecordedNodes));
    gRecordedNodeCount = 0;
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
    CodeMotionNode root;

    ResetState();
    COpt_00521a10();
    Check(gStageCount == 0, "null tree skipped");

    memset(&root, 0, sizeof(root));
    gCodeMotionTree_0058763c = &root;
    COpt_00521a10();
    Check(gStageCount == 0, "tree pass has no external action");
    Check(root.unknown_3c == -1, "tree pass initialized root");
}

static void TestTreeWalkOrder(void)
{
    CodeMotionNode root;
    CodeMotionNode child;
    CodeMotionNode sibling;

    ResetState();
    memset(&root, 0, sizeof(root));
    memset(&child, 0, sizeof(child));
    memset(&sibling, 0, sizeof(sibling));
    root.children = &child;
    root.sibling = &sibling;

    COpt_00521a30(&root);
    Check(root.unknown_3c == -1 && child.unknown_3c == -1 &&
              sibling.unknown_3c == -1,
          "first walker visits whole tree");

    ResetState();
    COpt_00524c10(&root);
    Check(gRecordedNodeCount == 3, "second postorder node count");
    Check(gRecordedNodes[0] == &child && gRecordedNodes[1] == &root &&
              gRecordedNodes[2] == &sibling,
          "second postorder traversal");

    ResetState();
    child.skip_leaf_pass_4f = 0;
    sibling.skip_leaf_pass_4f = 1;
    COpt_00525070(&root);
    Check(gRecordedNodeCount == 1 && gRecordedNodes[0] == &child,
          "eligible leaves only");
}

static void TestNodeSummary(void)
{
    CodeMotionNode node;
    PCodeBlock entry;
    PCodeBlock body;
    PCodeBlockLink entry_link;
    PCodeBlockLink body_link;
    PCodeBlockLink empty_predecessors;
    PCodeBlockLink empty_successors;
    PCodeBlockLink body_successors;
    PCodeBlockLink body_successor;
    PCodeInstruction instructions[5];
    int index;

    ResetState();
    memset(&node, 0, sizeof(node));
    memset(&entry, 0, sizeof(entry));
    memset(&body, 0, sizeof(body));
    memset(&entry_link, 0, sizeof(entry_link));
    memset(&body_link, 0, sizeof(body_link));
    memset(&empty_predecessors, 0, sizeof(empty_predecessors));
    memset(&empty_successors, 0, sizeof(empty_successors));
    memset(&body_successors, 0, sizeof(body_successors));
    memset(&body_successor, 0, sizeof(body_successor));
    memset(instructions, 0, sizeof(instructions));

    node.entry_block = &entry;
    node.blocks = &entry_link;
    node.unknown_50 = 1;
    node.unknown_51 = 1;
    node.unknown_55 = 1;
    node.unknown_56 = 1;
    entry_link.block = &entry;
    entry_link.next = &body_link;
    body_link.block = &body;
    entry.predecessors = &empty_predecessors;
    entry.successors = &empty_successors;
    body.predecessors = &empty_predecessors;
    body.successors = &body_successors;
    body_successors.next = &body_successor;
    entry.instruction_count = 2;
    body.instruction_count = 5;
    entry.flags_2e = 0x40;
    entry.instructions = &instructions[0];
    for (index = 0; index < 4; index++) {
        instructions[index].next = &instructions[index + 1];
    }
    instructions[0].flags = 0x4000;
    instructions[1].opcode = 0x12;
    instructions[2].flags = 0x08;
    instructions[2].opcode = 0x17;
    instructions[3].flags = 0x10;
    instructions[3].opcode = 0x2a;
    instructions[4].opcode = 0x85;

    COpt_00521bb0(&node);

    Check(node.instruction_count == 7, "instruction count accumulated");
    Check(node.unknown_3c == -1, "node sentinel initialized");
    Check(node.skip_leaf_pass_4f == 0, "connected non-entry block found");
    Check(node.has_call == 1, "call flag classified");
    Check(node.uses_count_register == 1, "count-register opcode classified");
    Check(node.has_block_flag_40 == 1, "block flag classified");
    Check(node.has_indexed_load == 1, "indexed load classified");
    Check(node.has_indexed_store == 1, "indexed store classified");
    Check(node.has_memory_barrier == 1, "memory barrier classified");
    Check(node.unknown_50 == 0 && node.unknown_51 == 0 &&
              node.unknown_55 == 0 && node.unknown_56 == 0,
          "node facts reset");
}

static void TestObjectTreeInsert(void)
{
    CompilerObject objects[3];
    CodeMotionObjectNode* allocation;
    int allocation_count;

    ResetState();
    memset(objects, 0, sizeof(objects));

    COpt_00524b20(&objects[1]);
    COpt_00524b20(&objects[0]);
    COpt_00524b20(&objects[2]);
    COpt_00524b20(&objects[1]);

    Check(gAllocationCount == 3, "one object node per unique object");
    Check(gCodeMotionObjectTree_005880ac->object == &objects[1],
          "first object is tree root");
    Check(gCodeMotionObjectTree_005880ac->left->object == &objects[0],
          "lower object inserted left");
    Check(gCodeMotionObjectTree_005880ac->right->object == &objects[2],
          "higher object inserted right");

    allocation_count = 0;
    for (allocation = gCodeMotionAllocationList_005870fc; allocation != 0;
         allocation = allocation->allocation_next)
    {
        Check(allocation->unknown_10 == 0 && allocation->unknown_14 == 0,
              "object-node state initialized");
        allocation_count++;
    }
    Check(allocation_count == 3, "all object nodes tracked for release");
}

static void TestObjectInstructionCompatibility(void)
{
    PCodeInstruction instruction;
    CompilerObject object;
    CompilerType wrapper;
    CompilerType type;
    RegisterInfo info;

    ResetState();
    memset(&instruction, 0, sizeof(instruction));
    memset(&object, 0, sizeof(object));
    memset(&wrapper, 0, sizeof(wrapper));
    memset(&type, 0, sizeof(type));
    memset(&info, 0, sizeof(info));
    object.type = &wrapper;
    object.register_info_26 = &info;
    wrapper.kind = 0x0c;
    wrapper.wrapped_type = &type;
    type.kind = 2;
    type.size = 4;

    instruction.opcode = 0x8e;
    Check(COpt_005248c0(&instruction, &object) == 1,
          "word operation accepts wrapped word type");
    type.size = 8;
    Check(COpt_005248c0(&instruction, &object) == 0,
          "word operation rejects wrapped doubleword type");

    instruction.opcode = 0x92;
    Check(COpt_005248c0(&instruction, &object) == 1,
          "wide operation accepts non-word scalar");

    object.kind = 2;
    instruction.opcode = 0;
    Check(COpt_005248c0(&instruction, &object) == 0,
          "ineligible object kind rejected");
    object.kind = 1;
    info.flags_22 = 1;
    Check(COpt_005248c0(&instruction, &object) == 1,
          "register-marked object bypasses type filter");

    object.kind = 0;
    object.type = &type;
    type.kind = 4;
    type.subtype = 5;
    instruction.opcode = 0xf7;
    Check(COpt_005248c0(&instruction, &object) == 1,
          "special operation accepts subtype range");
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
    int expected_stages[] = {3, 4, 5, 6, 7, 8};
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
    gCodeMotionAllocationList_005870fc = (CodeMotionObjectNode*) &object;
    gCodeMotionObjectTree_005880ac = (CodeMotionObjectNode*) &object;

    COpt_SetLoopCodeMotionMode(1);

    Check(gCodeMotionObjectTree_005880ac->object == &object,
          "candidate object collected");
    Check(gAllocationCount == 18,
          "object node, block state, and eight sets per block");
    Check(gAllocationSizes[0] == (int) sizeof(CodeMotionObjectNode),
          "object-node allocation");
    Check(gAllocationSizes[1] == 2 * 8 * (int) sizeof(void*),
          "two block-state records");
    for (index = 2; index < 18; index++) {
        if (((index - 2) & 7) < 4) {
            Check(gAllocationSizes[index] == 8, "definition-set size");
        } else {
            Check(gAllocationSizes[index] == 12, "use-set size");
        }
    }
    Check(gStageCount == 6, "setup stage count");
    for (index = 0; index < 6; index++) {
        Check(gStages[index] == expected_stages[index], "setup stage order");
    }
}

static void TestCoordinator(void)
{
    CodeMotionNode root;

    ResetState();
    memset(&root, 0, sizeof(root));
    gCodeMotionTree_0058763c = &root;
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
    TestTreeWalkOrder();
    TestNodeSummary();
    TestObjectTreeInsert();
    TestObjectInstructionCompatibility();
    TestSetup();
    TestCoordinator();
    puts("code-motion model tests passed");
    return 0;
}
