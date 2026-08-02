#ifndef MWCC_COPT_H
#define MWCC_COPT_H

typedef struct CodeMotionNode {
    struct CodeMotionNode* unknown_00;
    struct CodeMotionNode* sibling;  /* 0x04 */
    struct CodeMotionNode* children; /* 0x08 */
    struct PCodeBlock* entry_block;  /* 0x0c */
    unsigned char unknown_10[0x0c];
    struct PCodeBlockLink* blocks; /* 0x1c */
    unsigned char unknown_20[0x18];
    int instruction_count; /* 0x38 */
    int unknown_3c;
    unsigned char unknown_40[0x0d];
    unsigned char has_call;            /* 0x4d */
    unsigned char uses_count_register; /* 0x4e */
    unsigned char skip_leaf_pass_4f;   /* 0x4f */
    unsigned char unknown_50;          /* 0x50 */
    unsigned char unknown_51;          /* 0x51 */
    unsigned char has_block_flag_40;   /* 0x52 */
    unsigned char has_indexed_load;    /* 0x53 */
    unsigned char has_indexed_store;   /* 0x54 */
    unsigned char unknown_55;          /* 0x55 */
    unsigned char unknown_56;          /* 0x56 */
    unsigned char has_memory_barrier;  /* 0x57 */
} CodeMotionNode;

extern CodeMotionNode* gCodeMotionTree_0058763c;

void COpt_00521a10(void);
void COpt_00521a30(CodeMotionNode* node);
void COpt_00521bb0(CodeMotionNode* node);
void COpt_SetLoopCodeMotionMode(int mode);
void COpt_00524bd0(void);
void COpt_00524c10(CodeMotionNode* node);
void COpt_00525070(CodeMotionNode* node);

#ifndef MWCC_SKIP_LAYOUT_ASSERTS
#include <stddef.h>
typedef char CodeMotionNode_size_58[(sizeof(CodeMotionNode) == 0x58) ? 1 : -1];
typedef char CodeMotionNode_entry_0c
    [(offsetof(CodeMotionNode, entry_block) == 0x0c) ? 1 : -1];
typedef char
    CodeMotionNode_blocks_1c[(offsetof(CodeMotionNode, blocks) == 0x1c) ? 1
                                                                        : -1];
typedef char CodeMotionNode_instruction_count_38
    [(offsetof(CodeMotionNode, instruction_count) == 0x38) ? 1 : -1];
typedef char
    CodeMotionNode_facts_4d[(offsetof(CodeMotionNode, has_call) == 0x4d) ? 1
                                                                         : -1];
typedef char CodeMotionNode_barrier_57
    [(offsetof(CodeMotionNode, has_memory_barrier) == 0x57) ? 1 : -1];
#endif

#endif
