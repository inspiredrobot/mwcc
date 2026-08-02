/*
 * CodeMotion.c
 *
 * Initial target slice:
 *   0x00521a10  COpt_00521a10
 *   0x00521a30  COpt_00521a30
 *   0x00521bb0  COpt_00521bb0
 *   0x00523650  COpt_SetLoopCodeMotionMode
 *   0x00524bd0  COpt_00524bd0
 *
 * The source-file identity is confirmed by assertion strings referenced by
 * neighboring functions in the same target region. Address-suffixed names
 * remain where the exact operation has not yet been established.
 */

#include "mwcc/COpt.h"

#include "mwcc/backend_types.h"

typedef struct CodeMotionBlockState {
    unsigned int* definition_sets[4]; /* 0x00 */
    unsigned int* use_sets[4];        /* 0x10 */
} CodeMotionBlockState;

extern PCodeBlock* gPCodeBlocks; /* 0x00587c74 */
extern int gPCodeBlockCount;     /* 0x00587190 */

extern void* gCodeMotionAllocationList_005870fc;
extern void* gCodeMotionObjectTree_005880ac;
extern int gCodeMotionDefinitionCount_00587ebc;
extern int gCodeMotionUseCount_00587e38;
extern CodeMotionBlockState* gCodeMotionBlockState_00587fe4;
extern int gCodeMotionCounter_005880b8;
extern int gCodeMotionChanged; /* 0x005875b0 */

extern void* CodeMotion_Allocate(unsigned int size); /* 0x00441f20 */
extern void CodeMotion_FreeIteration(void);          /* 0x00441e20 */
extern void SpillCode_BuildBlockOrder(void);         /* 0x0049ce40 */

extern void COpt_005237f0(void);
extern void COpt_00523920(void);
extern void COpt_00523a50(void);
extern void COpt_005240b0(int mode);
extern void COpt_005246d0(int mode);
extern void COpt_00524b20(CompilerObject* object);
extern void COpt_00524d90(CodeMotionNode* node);
extern void COpt_00525200(CodeMotionNode* node);

static unsigned int* CodeMotion_AllocateBits(int bit_count)
{
    return CodeMotion_Allocate(((bit_count + 31) >> 5) * sizeof(unsigned int));
}

/* 0x00521a10; control-flow equivalent; 0.00% positional comparable match. */
void COpt_00521a10(void)
{
    if (gCodeMotionTree_0058763c != 0) {
        COpt_00521a30(gCodeMotionTree_0058763c);
    }
}

/* 0x00521a30; high-level equivalent; 13.73% comparable byte match. */
void COpt_00521a30(CodeMotionNode* node)
{
    for (; node != 0; node = node->sibling) {
        if (node->children != 0) {
            COpt_00521a30(node->children);
        }
        COpt_00521bb0(node);
    }
}

/* 0x00521bb0; instruction-exact; 100.00% comparable byte match. */
void COpt_00521bb0(CodeMotionNode* node)
{
    PCodeBlockLink* link;

    node->instruction_count = 0;
    node->has_call = 0;
    node->uses_count_register = 0;
    node->skip_leaf_pass_4f = 1;
    node->unknown_51 = 0;
    node->unknown_50 = 0;
    node->unknown_55 = 0;
    node->unknown_56 = 0;
    node->unknown_3c = -1;
    node->has_memory_barrier = 0;
    node->has_block_flag_40 = 0;

    for (link = node->blocks; link != 0; link = link->next) {
        PCodeBlock* block = link->block;
        PCodeInstruction* instruction;

        node->instruction_count += block->instruction_count;
        if (block != node->entry_block &&
            (block->successors->next != 0 || block->predecessors->next != 0))
        {
            node->skip_leaf_pass_4f = 0;
        }
        if ((block->flags_2e & 0x40) == 0x40) {
            node->has_block_flag_40 = 1;
        }

        for (instruction = block->instructions; instruction != 0;
             instruction = instruction->next)
        {
            if ((instruction->flags & 0x4000) != 0) {
                node->has_call = 1;
            }
            if (instruction->opcode == 0x13 || instruction->opcode == 0x12 ||
                instruction->opcode == 0x04 || instruction->opcode == 0x78 ||
                instruction->opcode == 0x80)
            {
                node->uses_count_register = 1;
            } else if ((instruction->flags & 0x08) != 0) {
                if (instruction->opcode == 0x17 ||
                    instruction->opcode == 0x1b ||
                    instruction->opcode == 0x1f ||
                    instruction->opcode == 0x24 ||
                    instruction->opcode == 0x90 || instruction->opcode == 0x94)
                {
                    node->has_indexed_load = 1;
                }
            } else if ((instruction->flags & 0x10) != 0) {
                if (instruction->opcode == 0x2a ||
                    instruction->opcode == 0x2e ||
                    instruction->opcode == 0x33 ||
                    instruction->opcode == 0x98 || instruction->opcode == 0x9c)
                {
                    node->has_indexed_store = 1;
                }
            } else if ((unsigned short) (instruction->opcode - 0x85) <= 2) {
                node->has_memory_barrier = 1;
            }
        }
    }
}

/* 0x00523650; high-level equivalent; 80.21% comparable byte match. */
void COpt_SetLoopCodeMotionMode(int mode)
{
    PCodeBlock* block;
    PCodeInstruction* instruction;
    CodeMotionBlockState* state;
    int index;

    if (mode != 0) {
        gCodeMotionAllocationList_005870fc = 0;
        gCodeMotionObjectTree_005880ac = gCodeMotionAllocationList_005870fc;

        for (block = gPCodeBlocks; block != 0; block = block->next) {
            for (instruction = block->instructions; instruction != 0;
                 instruction = instruction->next)
            {
                if ((instruction->flags & PCodeInstruction_GPRResultMask) !=
                        0 &&
                    (instruction->flags & PCodeInstruction_NullObjectMemory) ==
                        0)
                {
                    COpt_00524b20(instruction->operands[2].object);
                }
            }
        }
    }

    COpt_005246d0(mode);
    COpt_005240b0(mode);

    state =
        CodeMotion_Allocate(gPCodeBlockCount * sizeof(CodeMotionBlockState));
    gCodeMotionBlockState_00587fe4 = state;
    for (index = 0; index < gPCodeBlockCount; index++, state++) {
        state->definition_sets[0] =
            CodeMotion_AllocateBits(gCodeMotionDefinitionCount_00587ebc);
        state->definition_sets[1] =
            CodeMotion_AllocateBits(gCodeMotionDefinitionCount_00587ebc);
        state->definition_sets[2] =
            CodeMotion_AllocateBits(gCodeMotionDefinitionCount_00587ebc);
        state->definition_sets[3] =
            CodeMotion_AllocateBits(gCodeMotionDefinitionCount_00587ebc);
        state->use_sets[0] =
            CodeMotion_AllocateBits(gCodeMotionUseCount_00587e38);
        state->use_sets[1] =
            CodeMotion_AllocateBits(gCodeMotionUseCount_00587e38);
        state->use_sets[2] =
            CodeMotion_AllocateBits(gCodeMotionUseCount_00587e38);
        state->use_sets[3] =
            CodeMotion_AllocateBits(gCodeMotionUseCount_00587e38);
    }

    COpt_00523a50();
    SpillCode_BuildBlockOrder();
    COpt_00523920();
    COpt_005237f0();
}

/* 0x00524bd0; control-flow equivalent; 20.69% comparable byte match. */
void COpt_00524bd0(void)
{
    gCodeMotionCounter_005880b8 = 0;
    gCodeMotionChanged = 0;
    if (gCodeMotionTree_0058763c != 0) {
        COpt_00524c10(gCodeMotionTree_0058763c);
        COpt_00525070(gCodeMotionTree_0058763c);
    }
    CodeMotion_FreeIteration();
}

/* 0x00524c10; high-level equivalent; 13.73% comparable byte match. */
void COpt_00524c10(CodeMotionNode* node)
{
    for (; node != 0; node = node->sibling) {
        if (node->children != 0) {
            COpt_00524c10(node->children);
        }
        COpt_00524d90(node);
    }
}

/* 0x00525070; high-level equivalent; 20.83% comparable byte match. */
void COpt_00525070(CodeMotionNode* node)
{
    for (; node != 0; node = node->sibling) {
        if (node->children != 0) {
            COpt_00525070(node->children);
        } else if (node->skip_leaf_pass_4f == 0) {
            COpt_00525200(node);
        }
    }
}
