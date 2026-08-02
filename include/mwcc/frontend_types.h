#ifndef MWCC_FRONTEND_TYPES_H
#define MWCC_FRONTEND_TYPES_H

#include "mwcc/backend_types.h"

#include <stddef.h>

#pragma pack(push, 2)

typedef struct CExpression CExpression;

typedef struct CExpressionList {
    struct CExpressionList* next; /* 0x00 */
    CExpression* expression;      /* 0x04 */
} CExpressionList;

typedef union CExpressionValue {
    CExpression* expression;
    CExpressionList* list;
    CompilerObject* object;
    void* pointer;
} CExpressionValue;

struct CExpression {
    unsigned char kind; /* 0x00 */
    unsigned char unknown_01[9];
    CExpressionValue value_0a; /* 0x0a */
    CExpressionValue value_0e; /* 0x0e */
    CExpressionValue value_12; /* 0x12 */
};

typedef struct CodeGenItem {
    struct CodeGenItem* next; /* 0x00 */
    unsigned char kind;       /* 0x04 */
    unsigned char byte_05;
    unsigned char flags_06;
    unsigned char byte_07;
    unsigned short value_08;
    CExpression* expression_0a;
    void* pointer_0e;
    unsigned char unknown_12[4];
    void* pointer_16;
} CodeGenItem;

#pragma pack(pop)

enum CExpressionKind {
    CExpressionKind_ObjectReference = 0x38
};

#ifndef MWCC_SKIP_LAYOUT_ASSERTS
typedef char
    CExpression_value_0a[(offsetof(CExpression, value_0a) == 0x0a) ? 1 : -1];
typedef char
    CExpression_value_0e[(offsetof(CExpression, value_0e) == 0x0e) ? 1 : -1];
typedef char
    CExpression_value_12[(offsetof(CExpression, value_12) == 0x12) ? 1 : -1];
typedef char CExpressionList_expression_04
    [(offsetof(CExpressionList, expression) == 0x04) ? 1 : -1];
typedef char CodeGenItem_expression_0a
    [(offsetof(CodeGenItem, expression_0a) == 0x0a) ? 1 : -1];
typedef char CodeGenItem_pointer_16[(offsetof(CodeGenItem, pointer_16) == 0x16)
                                        ? 1
                                        : -1];
#endif

#endif
