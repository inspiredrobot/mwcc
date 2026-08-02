#ifndef MWCC_COPT_H
#define MWCC_COPT_H

typedef struct CodeMotionNode {
    struct CodeMotionNode* unknown_00;
    struct CodeMotionNode* sibling;  /* 0x04 */
    struct CodeMotionNode* children; /* 0x08 */
    unsigned char unknown_0c[0x43];
    unsigned char skip_leaf_pass_4f; /* 0x4f */
} CodeMotionNode;

extern CodeMotionNode* gCodeMotionTree_0058763c;

void COpt_00521a10(void);
void COpt_00521a30(CodeMotionNode* node);
void COpt_SetLoopCodeMotionMode(int mode);
void COpt_00524bd0(void);
void COpt_00524c10(CodeMotionNode* node);
void COpt_00525070(CodeMotionNode* node);

#endif
