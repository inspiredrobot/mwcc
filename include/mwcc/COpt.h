#ifndef MWCC_COPT_H
#define MWCC_COPT_H

struct CompilerObject;
struct PCodeInstruction;

#pragma pack(push, 2)
typedef union CodeMotionEntryValue {
    short reg;
    unsigned int raw;
    struct CompilerObject* object;
} CodeMotionEntryValue;

typedef struct CodeMotionEntry {
    struct PCodeInstruction* instruction; /* 0x00 */
    unsigned char kind;                   /* 0x04 */
    unsigned char is_implicit;            /* 0x05 */
    CodeMotionEntryValue value;           /* 0x06 */
} CodeMotionEntry;

typedef struct CodeMotionEntryLink {
    struct CodeMotionEntryLink* next; /* 0x00 */
    int entry_index;                  /* 0x04 */
} CodeMotionEntryLink;
#pragma pack(pop)

typedef struct CodeMotionBlockState {
    unsigned int* definition_sets[4]; /* 0x00 */
    unsigned int* use_sets[4];        /* 0x10 */
} CodeMotionBlockState;

typedef struct CodeMotionNode {
    struct CodeMotionNode* unknown_00;
    struct CodeMotionNode* sibling;  /* 0x04 */
    struct CodeMotionNode* children; /* 0x08 */
    struct PCodeBlock* entry_block;  /* 0x0c */
    unsigned char unknown_10[0x0c];
    struct PCodeBlockLink* blocks; /* 0x1c */
    unsigned char unknown_20[8];
    unsigned int* block_membership; /* 0x28 */
    unsigned char unknown_2c[0x0c];
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

typedef struct CodeMotionObjectNode {
    struct CodeMotionObjectNode* allocation_next; /* 0x00 */
    struct CodeMotionObjectNode* left;            /* 0x04 */
    struct CodeMotionObjectNode* right;           /* 0x08 */
    struct CompilerObject* object;                /* 0x0c */
    CodeMotionEntryLink* use_entries;             /* 0x10 */
    CodeMotionEntryLink* definition_entries;      /* 0x14 */
} CodeMotionObjectNode;

extern CodeMotionNode* gCodeMotionTree_0058763c;
extern CodeMotionObjectNode* gCodeMotionAllocationList_005870fc;
extern CodeMotionObjectNode* gCodeMotionObjectTree_005880ac;
extern CodeMotionBlockState* gCodeMotionBlockState_00587fe4;

void COpt_00521a10(void);
void COpt_00521a30(CodeMotionNode* node);
void COpt_00521bb0(CodeMotionNode* node);
void COpt_SetLoopCodeMotionMode(int mode);
void COpt_005240b0(int include_implicit);
void COpt_005246d0(int include_implicit);
int COpt_005248c0(struct PCodeInstruction* instruction,
                  struct CompilerObject* object);
void COpt_00524b20(struct CompilerObject* object);
CodeMotionObjectNode* COpt_00524b90(struct CompilerObject* object);
void COpt_00524bd0(void);
void COpt_00524c10(CodeMotionNode* node);
void COpt_00524d90(CodeMotionNode* node);
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
typedef char CodeMotionNode_block_membership_28
    [(offsetof(CodeMotionNode, block_membership) == 0x28) ? 1 : -1];
typedef char
    CodeMotionNode_facts_4d[(offsetof(CodeMotionNode, has_call) == 0x4d) ? 1
                                                                         : -1];
typedef char CodeMotionNode_barrier_57
    [(offsetof(CodeMotionNode, has_memory_barrier) == 0x57) ? 1 : -1];
typedef char
    CodeMotionObjectNode_size_18[(sizeof(CodeMotionObjectNode) == 0x18) ? 1
                                                                        : -1];
typedef char CodeMotionObjectNode_object_0c
    [(offsetof(CodeMotionObjectNode, object) == 0x0c) ? 1 : -1];
typedef char
    CodeMotionEntry_size_0a[(sizeof(CodeMotionEntry) == 0x0a) ? 1 : -1];
typedef char
    CodeMotionEntry_value_06[(offsetof(CodeMotionEntry, value) == 0x06) ? 1
                                                                        : -1];
typedef char CodeMotionEntryLink_size_08[(sizeof(CodeMotionEntryLink) == 0x08)
                                             ? 1
                                             : -1];
#endif

#endif
