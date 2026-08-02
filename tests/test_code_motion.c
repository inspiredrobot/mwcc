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
CodeMotionBlockState* gCodeMotionBlockState_00587fe4;
int gCodeMotionCounter_005880b8;
int gCodeMotionChanged;
short gUsedVirtualRegistersGPR;
short gUsedVirtualRegistersFPR;
short gUsedVirtualRegistersVR;
CodeMotionEntry* gCodeMotionUseEntries_00587650;
CodeMotionEntry* gCodeMotionDefinitionEntries_00587588;
CodeMotionEntryLink** gCodeMotionGPRUseEntries_00587f14;
CodeMotionEntryLink** gCodeMotionGPRDefinitionEntries_00587ed4;
CodeMotionEntryLink** gCodeMotionFPRUseEntries_00587ee8;
CodeMotionEntryLink** gCodeMotionFPRDefinitionEntries_00587f04;
CodeMotionEntryLink** gCodeMotionVRUseEntries_00587c88;
CodeMotionEntryLink** gCodeMotionVRDefinitionEntries_005876f0;

static unsigned int gAllocationPool[1024];
static int gAllocationOffset;
static int gAllocationSizes[32];
static int gAllocationCount;
static int gStages[32];
static int gStageCount;
static CodeMotionNode* gRecordedNodes[32];
static int gRecordedNodeCount;
static int gDirectCandidate;
static int gDefinitionCandidate;
static int gEntryCandidate;
static int gFallbackCandidate;
static int gMoveCount;
static int gCopyCount;

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
    Check(gAllocationOffset <= 1024, "allocation pool");
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

void CodeMotion_CopyBits(unsigned int* destination, const unsigned int* source,
                         int bit_count)
{
    int index;

    gCopyCount++;
    for (index = 0; index < (bit_count + 31) / 32; index++) {
        destination[index] = source[index];
    }
}

int COpt_00525fc0(PCodeInstruction* instruction, CodeMotionNode* node,
                  unsigned int* available_definitions)
{
    (void) instruction;
    (void) node;
    (void) available_definitions;
    return gFallbackCandidate;
}

void COpt_00526230(PCodeInstruction* instruction, CodeMotionNode* node)
{
    (void) node;
    gMoveCount++;
    instruction->operand_count = 0;
}

int COpt_00526500(unsigned char* definition, CodeMotionNode* node)
{
    (void) definition;
    (void) node;
    return gEntryCandidate;
}

int COpt_005266e0(int definition_index, CodeMotionNode* node)
{
    (void) definition_index;
    (void) node;
    return gDefinitionCandidate;
}

int COpt_00526b50(PCodeInstruction* instruction, CodeMotionNode* node)
{
    (void) instruction;
    (void) node;
    return gDirectCandidate;
}

int COpt_00526d80(PCodeInstruction* instruction, CodeMotionNode* node,
                  unsigned int* available_definitions, int arg_3, int arg_4)
{
    (void) instruction;
    (void) node;
    (void) available_definitions;
    Check(arg_3 == 0 && arg_4 == 0, "motion analysis arguments");
    return gDirectCandidate;
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
    gDirectCandidate = 0;
    gDefinitionCandidate = 0;
    gEntryCandidate = 0;
    gFallbackCandidate = 0;
    gMoveCount = 0;
    gCopyCount = 0;
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
    gUsedVirtualRegistersGPR = 0;
    gUsedVirtualRegistersFPR = 0;
    gUsedVirtualRegistersVR = 0;
    gCodeMotionUseEntries_00587650 = 0;
    gCodeMotionDefinitionEntries_00587588 = 0;
    gCodeMotionGPRUseEntries_00587f14 = 0;
    gCodeMotionGPRDefinitionEntries_00587ed4 = 0;
    gCodeMotionFPRUseEntries_00587ee8 = 0;
    gCodeMotionFPRDefinitionEntries_00587f04 = 0;
    gCodeMotionVRUseEntries_00587c88 = 0;
    gCodeMotionVRDefinitionEntries_005876f0 = 0;
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
    Check(gAllocationCount == 3, "second postorder node count");

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
    Check(COpt_00524b90(&objects[0]) == gCodeMotionObjectTree_005880ac->left,
          "lower object found");
    Check(COpt_00524b90(&objects[1]) == gCodeMotionObjectTree_005880ac,
          "root object found");
    Check(COpt_00524b90(&objects[2]) == gCodeMotionObjectTree_005880ac->right,
          "higher object found");

    allocation_count = 0;
    for (allocation = gCodeMotionAllocationList_005870fc; allocation != 0;
         allocation = allocation->allocation_next)
    {
        Check(allocation->use_entries == 0 &&
                  allocation->definition_entries == 0,
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

static void TestDefUseCensus(void)
{
    typedef struct TestInstruction {
        PCodeInstruction instruction;
        PCodeOperand extra_operands[2];
    } TestInstruction;

    PCodeBlock block;
    TestInstruction instructions[3];
    CompilerObject objects[2];
    CodeMotionObjectNode object_nodes[2];

    ResetState();
    memset(&block, 0, sizeof(block));
    memset(instructions, 0, sizeof(instructions));
    memset(objects, 0, sizeof(objects));
    memset(object_nodes, 0, sizeof(object_nodes));
    block.instructions = &instructions[0].instruction;
    gPCodeBlocks = &block;
    instructions[0].instruction.next = &instructions[1].instruction;
    instructions[1].instruction.next = &instructions[2].instruction;
    instructions[0].instruction.operand_count = 3;
    instructions[0].instruction.operands[0].kind = 0;
    instructions[0].instruction.operands[0].flags = PCodeOperand_Use;
    instructions[0].instruction.operands[0].value.reg = 32;
    instructions[0].instruction.operands[1].kind = 1;
    instructions[0].instruction.operands[1].flags = PCodeOperand_Definition;
    instructions[0].instruction.operands[1].value.reg = 33;
    instructions[0].instruction.operands[2].kind = 9;
    instructions[0].instruction.operands[2].flags =
        PCodeOperand_Use | PCodeOperand_Definition;
    instructions[0].instruction.operands[2].value.reg = 31;
    instructions[1].instruction.operand_count = 1;
    instructions[1].instruction.flags =
        PCodeInstruction_ImplicitUse | PCodeInstruction_NullObjectMemory;
    instructions[2].instruction.operand_count = 1;
    instructions[2].instruction.flags = PCodeInstruction_GPRFixedRange;
    objects[0].kind = 0;
    objects[1].kind = 2;
    object_nodes[0].object = &objects[0];
    object_nodes[0].allocation_next = &object_nodes[1];
    object_nodes[1].object = &objects[1];
    gCodeMotionAllocationList_005870fc = &object_nodes[0];

    COpt_005246d0(0);
    Check(gCodeMotionUseCount_00587e38 == 1 &&
              gCodeMotionDefinitionCount_00587ebc == 1,
          "explicit def-use census");
    Check(instructions[0].instruction.first_use_index == 0 &&
              instructions[0].instruction.first_definition_index == 0,
          "first instruction index base");
    Check(instructions[1].instruction.first_use_index == 1 &&
              instructions[1].instruction.first_definition_index == 1,
          "second instruction index base");

    COpt_005246d0(1);
    Check(gCodeMotionUseCount_00587e38 == 3 &&
              gCodeMotionDefinitionCount_00587ebc == 2,
          "implicit object def-use census");

    gUsedVirtualRegistersGPR = 34;
    gUsedVirtualRegistersFPR = 34;
    gUsedVirtualRegistersVR = 34;
    COpt_005240b0(1);
    Check(gCodeMotionUseEntries_00587650[0].instruction ==
                  &instructions[0].instruction &&
              gCodeMotionUseEntries_00587650[0].kind == 0 &&
              gCodeMotionUseEntries_00587650[0].value.reg == 32,
          "explicit use entry materialized");
    Check(gCodeMotionDefinitionEntries_00587588[0].instruction ==
                  &instructions[0].instruction &&
              gCodeMotionDefinitionEntries_00587588[0].kind == 1 &&
              gCodeMotionDefinitionEntries_00587588[0].value.reg == 33,
          "explicit definition entry materialized");
    Check(gCodeMotionUseEntries_00587650[1].is_implicit == 1 &&
              gCodeMotionUseEntries_00587650[1].value.object == &objects[0],
          "compatible implicit use materialized");
    Check(gCodeMotionUseEntries_00587650[2].value.object == &objects[0] &&
              gCodeMotionDefinitionEntries_00587588[1].value.object ==
                  &objects[0],
          "shared implicit use-definition materialized");
    Check(gCodeMotionGPRUseEntries_00587f14[32]->entry_index == 0,
          "GPR use reverse index");
    Check(gCodeMotionFPRDefinitionEntries_00587f04[33]->entry_index == 0,
          "FPR definition reverse index");
    Check(object_nodes[0].use_entries != 0 &&
              object_nodes[0].definition_entries != 0,
          "object reverse indices");
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
    int expected_stages[] = {5, 6, 7, 8};
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
    Check(gAllocationCount == 27,
          "object entries, block state, and eight sets per block");
    Check(gAllocationSizes[0] == (int) sizeof(CodeMotionObjectNode),
          "object-node allocation");
    Check(gAllocationSizes[10] == 2 * 8 * (int) sizeof(void*),
          "two block-state records");
    for (index = 11; index < 27; index++) {
        if (((index - 11) & 7) < 4) {
            Check(gAllocationSizes[index] == 0, "definition-set size");
        } else {
            Check(gAllocationSizes[index] == 4, "use-set size");
        }
    }
    Check(gStageCount == 4, "setup stage count");
    for (index = 0; index < 4; index++) {
        Check(gStages[index] == expected_stages[index], "setup stage order");
    }
}

static void TestLoopNodeMotion(void)
{
    CodeMotionNode node;
    PCodeBlock block;
    PCodeBlockLink block_link;
    PCodeInstruction instruction;
    PCodeInstruction other_instruction;
    CodeMotionBlockState block_state;
    CodeMotionEntry entries[3];
    CodeMotionEntryLink first_reverse;
    CodeMotionEntryLink second_reverse;
    CodeMotionEntryLink* gpr_heads[33];
    unsigned int block_definitions;
    unsigned int membership;

    ResetState();
    memset(&node, 0, sizeof(node));
    memset(&block, 0, sizeof(block));
    memset(&block_link, 0, sizeof(block_link));
    memset(&instruction, 0, sizeof(instruction));
    memset(&other_instruction, 0, sizeof(other_instruction));
    memset(&block_state, 0, sizeof(block_state));
    memset(entries, 0, sizeof(entries));
    memset(gpr_heads, 0, sizeof(gpr_heads));

    block_definitions = 7;
    membership = 1;
    block_state.definition_sets[2] = &block_definitions;
    gCodeMotionBlockState_00587fe4 = &block_state;
    gCodeMotionDefinitionCount_00587ebc = 3;
    gCodeMotionDefinitionEntries_00587588 = entries;
    gCodeMotionGPRDefinitionEntries_00587ed4 = gpr_heads;

    first_reverse.next = &second_reverse;
    first_reverse.entry_index = 0;
    second_reverse.next = 0;
    second_reverse.entry_index = 2;
    gpr_heads[32] = &first_reverse;

    entries[1].instruction = &instruction;
    entries[1].kind = 0;
    entries[1].value.reg = 32;
    entries[2].instruction = &other_instruction;
    instruction.block = &block;
    instruction.first_definition_index = 1;
    instruction.flags = PCodeInstruction_NullObjectMemory;
    instruction.operand_count = 1;
    block.instructions = &instruction;
    block_link.block = &block;
    node.blocks = &block_link;
    node.block_membership = &membership;

    COpt_00524d90(&node);
    Check(gCopyCount == 1, "one fixed-point pass without motion");
    Check(gMoveCount == 0, "rejected instruction not moved");
    Check(gAllocationPool[0] == 2,
          "definition transfer kills peers and generates current entry");

    ResetState();
    memset(&node, 0, sizeof(node));
    memset(&block, 0, sizeof(block));
    memset(&block_link, 0, sizeof(block_link));
    memset(&instruction, 0, sizeof(instruction));
    memset(&block_state, 0, sizeof(block_state));
    memset(entries, 0, sizeof(entries));
    memset(gpr_heads, 0, sizeof(gpr_heads));
    block_definitions = 0;
    membership = 1;
    block_state.definition_sets[2] = &block_definitions;
    gCodeMotionBlockState_00587fe4 = &block_state;
    gCodeMotionDefinitionCount_00587ebc = 1;
    gCodeMotionDefinitionEntries_00587588 = entries;
    gCodeMotionGPRDefinitionEntries_00587ed4 = gpr_heads;
    entries[0].instruction = &instruction;
    entries[0].kind = 0;
    entries[0].value.reg = 32;
    instruction.block = &block;
    instruction.operand_count = 1;
    block.instructions = &instruction;
    block_link.block = &block;
    node.blocks = &block_link;
    node.block_membership = &membership;
    gDirectCandidate = 1;
    gDefinitionCandidate = 1;

    COpt_00524d90(&node);
    Check(gMoveCount == 1, "eligible instruction moved once");
    Check(gCopyCount == 2, "motion triggers another fixed-point pass");
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
    Check(gStageCount == 2, "coordinator stage count");
    Check(gStages[0] == 11 && gStages[1] == 9, "coordinator stage order");
}

int main(void)
{
    TestGuardedTreePass();
    TestTreeWalkOrder();
    TestNodeSummary();
    TestObjectTreeInsert();
    TestObjectInstructionCompatibility();
    TestDefUseCensus();
    TestSetup();
    TestLoopNodeMotion();
    TestCoordinator();
    puts("code-motion model tests passed");
    return 0;
}
