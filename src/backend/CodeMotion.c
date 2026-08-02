/*
 * CodeMotion.c
 *
 * Initial target slice:
 *   0x00521a10  COpt_00521a10
 *   0x00523650  COpt_SetLoopCodeMotionMode
 *   0x00524bd0  COpt_00524bd0
 *
 * The source-file identity is confirmed by assertion strings referenced by
 * neighboring functions in the same target region. Address-suffixed names
 * remain where the exact operation has not yet been established.
 */

#include "mwcc/COpt.h"

#include "mwcc/backend_types.h"

struct CodeMotionNode {
    struct CodeMotionNode* next;     /* 0x00 */
    struct CodeMotionNode* sibling;  /* 0x04 */
    struct CodeMotionNode* children; /* 0x08 */
};

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

extern void COpt_00521a30(CodeMotionNode* node);
extern void COpt_005237f0(void);
extern void COpt_00523920(void);
extern void COpt_00523a50(void);
extern void COpt_005240b0(int mode);
extern void COpt_005246d0(int mode);
extern void COpt_00524b20(CompilerObject* object);
extern void COpt_00524c10(CodeMotionNode* node);
extern void COpt_00525070(CodeMotionNode* node);

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
