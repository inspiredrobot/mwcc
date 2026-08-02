#ifndef MWCC_BACKEND_TYPES_H
#define MWCC_BACKEND_TYPES_H

#include <stddef.h>

#pragma pack(push, 2)

typedef struct CompilerType {
    unsigned char kind; /* 0x00 */
    unsigned char unknown_01;
    unsigned int size; /* 0x02 */
    unsigned char unknown_06[4];
    unsigned int flags_0a; /* 0x0a */
    signed char subtype;   /* 0x0e */
} CompilerType;

typedef struct PCodeFunctionSignature {
    unsigned char unknown_00[0x0e];
    CompilerType* result_type; /* 0x0e */
} PCodeFunctionSignature;

typedef struct PCodeFunction {
    unsigned char unknown_00[0x0e];
    PCodeFunctionSignature* signature; /* 0x0e */
} PCodeFunction;

typedef struct RegisterInfo {
    unsigned char unknown_00[0x24];
    short physical_register;  /* 0x24 */
    short secondary_register; /* 0x26 */
    unsigned char is_fpr;     /* 0x28 */
    unsigned char unknown_29;
    unsigned char is_vector; /* 0x2a */
    unsigned char unknown_2b;
} RegisterInfo;

typedef struct CompilerObject {
    unsigned char object_tag; /* 0x00: PCode memory operands require 5 */
    unsigned char unknown_01;
    unsigned char kind; /* 0x02 */
    unsigned char unknown_03[0x0b];
    CompilerType* type;    /* 0x0e */
    unsigned int flags_12; /* 0x12 */
    unsigned char unknown_16[0x10];
    RegisterInfo* register_info_26; /* 0x26 */
    unsigned char unknown_2a[4];
    RegisterInfo* register_info_2e; /* 0x2e */
} CompilerObject;

typedef struct ObjectList {
    struct ObjectList* next; /* 0x00 */
    CompilerObject* object;  /* 0x04 */
} ObjectList;

typedef union PCodeOperandValue {
    short reg;
    int signed_value;
    unsigned int unsigned_value;
} PCodeOperandValue;

typedef struct PCodeOperand {
    unsigned char kind;      /* 0x00 */
    unsigned char flags;     /* 0x01 */
    PCodeOperandValue value; /* 0x02 */
    CompilerObject* object;  /* 0x06 */
    unsigned char unknown_0a[2];
} PCodeOperand;

typedef struct PCodeOpcodeDescriptor {
    const char* mnemonic;        /* 0x00 */
    const char* operand_format;  /* 0x04 */
    unsigned char operand_count; /* 0x08: fixed operand count */
    unsigned char unknown_09;
    unsigned short flags;  /* 0x0a */
    unsigned int encoding; /* 0x0c: base PowerPC instruction encoding */
} PCodeOpcodeDescriptor;

enum PCodeOperandFlags {
    PCodeOperand_Use = 0x01,
    PCodeOperand_Definition = 0x02,
    PCodeOperand_LastUse = 0x04
};

enum PCodeInstructionFlags {
    PCodeInstruction_NullObjectMemory = 0x0040,
    PCodeInstruction_CloneExtraOperandExcluded = 0x0080,
    PCodeInstruction_CloneExtraOperand = 0x0200,
    PCodeInstruction_CoalesceDisabled = 0x0400,
    PCodeInstruction_CopySourceExclusion = 0x0800,
    PCodeInstruction_GPRFixedRange = 0x0020,
    PCodeInstruction_GPRPairInterference = 0x8000,
    PCodeInstruction_ObjectFlag1 = 0x10000,
    PCodeInstruction_ObjectFlag2 = 0x20000
};

enum PCodeInstructionFlagMasks {
    PCodeInstruction_GPRResultMask = 0x0018,
    PCodeInstruction_DeadCodeBarrierMask = 0x00020434
};

typedef struct PCodeInstructionContext {
    unsigned char unknown_00[0x2e];
    unsigned short flags; /* 0x2e */
} PCodeInstructionContext;

typedef struct PCodeInstruction {
    struct PCodeInstruction* next;     /* 0x00 */
    struct PCodeInstruction* previous; /* 0x04 */
    PCodeInstructionContext* context;  /* 0x08 */
    unsigned char unknown_0c[8];
    short opcode;             /* 0x14 */
    unsigned int flags;       /* 0x16 */
    short operand_count;      /* 0x1a */
    PCodeOperand operands[1]; /* 0x1c: variable-length array */
} PCodeInstruction;

typedef struct PCodeBlockLink {
    struct PCodeBlockLink* next; /* 0x00 */
    struct PCodeBlock* block;    /* 0x04 */
} PCodeBlockLink;

typedef struct PCodeBlock {
    struct PCodeBlock* next; /* 0x00 */
    unsigned char unknown_04[0x0c];
    PCodeBlockLink* successors;             /* 0x10 */
    PCodeInstruction* instructions;         /* 0x14 */
    PCodeInstruction* reverse_instructions; /* 0x18 */
    int index;                              /* 0x1c */
    unsigned char unknown_20[8];
    int execution_weight; /* 0x28 */
} PCodeBlock;

typedef struct PCodeBlockLiveness {
    unsigned int* use;      /* 0x00 */
    unsigned int* def;      /* 0x04 */
    unsigned int* live_in;  /* 0x08 */
    unsigned int* live_out; /* 0x0c */
} PCodeBlockLiveness;

typedef struct InterferenceNode {
    struct InterferenceNode* next; /* 0x00: temporary allocator lists */
    CompilerObject* object;        /* 0x04 */
    int spill_cost;                /* 0x08 */
    short virtual_register;        /* 0x0c */
    short degree;                  /* 0x0e */
    short physical_register;       /* 0x10 */
    unsigned char flags;           /* 0x12 */
    unsigned char unknown_13;
    short neighbor_count; /* 0x14 */
    short neighbors[1];   /* 0x16: variable-length array */
} InterferenceNode;

#pragma pack(pop)

#ifndef MWCC_SKIP_LAYOUT_ASSERTS
typedef char RegisterInfo_size_2c[(sizeof(RegisterInfo) == 0x2c) ? 1 : -1];
typedef char RegisterInfo_physical_24
    [(offsetof(RegisterInfo, physical_register) == 0x24) ? 1 : -1];
typedef char RegisterInfo_secondary_26
    [(offsetof(RegisterInfo, secondary_register) == 0x26) ? 1 : -1];
typedef char
    RegisterInfo_is_fpr_28[(offsetof(RegisterInfo, is_fpr) == 0x28) ? 1 : -1];
typedef char RegisterInfo_is_vector_2a
    [(offsetof(RegisterInfo, is_vector) == 0x2a) ? 1 : -1];
typedef char
    CompilerObject_type_0e[(offsetof(CompilerObject, type) == 0x0e) ? 1 : -1];
typedef char
    CompilerType_size_02[(offsetof(CompilerType, size) == 0x02) ? 1 : -1];
typedef char
    CompilerType_flags_0a[(offsetof(CompilerType, flags_0a) == 0x0a) ? 1 : -1];
typedef char CompilerType_subtype_0e[(offsetof(CompilerType, subtype) == 0x0e)
                                         ? 1
                                         : -1];
typedef char PCodeFunction_signature_0e
    [(offsetof(PCodeFunction, signature) == 0x0e) ? 1 : -1];
typedef char PCodeFunctionSignature_result_0e
    [(offsetof(PCodeFunctionSignature, result_type) == 0x0e) ? 1 : -1];
typedef char CompilerObject_info_26
    [(offsetof(CompilerObject, register_info_26) == 0x26) ? 1 : -1];
typedef char
    CompilerObject_flags_12[(offsetof(CompilerObject, flags_12) == 0x12) ? 1
                                                                         : -1];
typedef char CompilerObject_info_2e
    [(offsetof(CompilerObject, register_info_2e) == 0x2e) ? 1 : -1];
typedef char InterferenceNode_physical_10
    [(offsetof(InterferenceNode, physical_register) == 0x10) ? 1 : -1];
typedef char InterferenceNode_flags_12
    [(offsetof(InterferenceNode, flags) == 0x12) ? 1 : -1];
typedef char InterferenceNode_neighbors_16
    [(offsetof(InterferenceNode, neighbors) == 0x16) ? 1 : -1];
typedef char PCodeOperand_size_0c[(sizeof(PCodeOperand) == 0x0c) ? 1 : -1];
typedef char
    PCodeOperand_value_02[(offsetof(PCodeOperand, value) == 0x02) ? 1 : -1];
typedef char
    PCodeOperand_object_06[(offsetof(PCodeOperand, object) == 0x06) ? 1 : -1];
typedef char PCodeOpcodeDescriptor_size_10
    [(sizeof(PCodeOpcodeDescriptor) == 0x10) ? 1 : -1];
typedef char PCodeOpcodeDescriptor_format_04
    [(offsetof(PCodeOpcodeDescriptor, operand_format) == 0x04) ? 1 : -1];
typedef char PCodeOpcodeDescriptor_count_08
    [(offsetof(PCodeOpcodeDescriptor, operand_count) == 0x08) ? 1 : -1];
typedef char PCodeOpcodeDescriptor_flags_0a
    [(offsetof(PCodeOpcodeDescriptor, flags) == 0x0a) ? 1 : -1];
typedef char PCodeOpcodeDescriptor_encoding_0c
    [(offsetof(PCodeOpcodeDescriptor, encoding) == 0x0c) ? 1 : -1];
typedef char PCodeInstruction_operands_1c
    [(offsetof(PCodeInstruction, operands) == 0x1c) ? 1 : -1];
typedef char PCodeInstructionContext_flags_2e
    [(offsetof(PCodeInstructionContext, flags) == 0x2e) ? 1 : -1];
typedef char PCodeBlock_instructions_14
    [(offsetof(PCodeBlock, instructions) == 0x14) ? 1 : -1];
typedef char
    PCodeBlock_successors_10[(offsetof(PCodeBlock, successors) == 0x10) ? 1
                                                                        : -1];
typedef char PCodeBlock_weight_28
    [(offsetof(PCodeBlock, execution_weight) == 0x28) ? 1 : -1];
typedef char PCodeBlockLiveness_live_out_0c
    [(offsetof(PCodeBlockLiveness, live_out) == 0x0c) ? 1 : -1];
#endif

#endif
