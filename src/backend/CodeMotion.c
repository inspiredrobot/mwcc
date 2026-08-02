/*
 * CodeMotion.c
 *
 * Initial target slice:
 *   0x00521a10  COpt_00521a10
 *   0x00521a30  COpt_00521a30
 *   0x00521bb0  COpt_00521bb0
 *   0x00523650  COpt_SetLoopCodeMotionMode
 *   0x005240b0  COpt_005240b0
 *   0x005246d0  COpt_005246d0
 *   0x005248c0  COpt_005248c0
 *   0x00524bd0  COpt_00524bd0
 *   0x00524d90  COpt_00524d90
 *
 * The source-file identity is confirmed by assertion strings referenced by
 * neighboring functions in the same target region. Address-suffixed names
 * remain where the exact operation has not yet been established.
 */

#include "mwcc/COpt.h"

#include "mwcc/backend_types.h"

extern PCodeBlock* gPCodeBlocks; /* 0x00587c74 */
extern int gPCodeBlockCount;     /* 0x00587190 */

extern int gCodeMotionDefinitionCount_00587ebc;
extern int gCodeMotionUseCount_00587e38;
extern int gCodeMotionCounter_005880b8;
extern int gCodeMotionChanged; /* 0x005875b0 */
extern short gUsedVirtualRegistersGPR;
extern short gUsedVirtualRegistersFPR;
extern short gUsedVirtualRegistersVR;
extern CodeMotionEntry* gCodeMotionUseEntries_00587650;
extern CodeMotionEntry* gCodeMotionDefinitionEntries_00587588;
extern CodeMotionEntryLink** gCodeMotionGPRUseEntries_00587f14;
extern CodeMotionEntryLink** gCodeMotionGPRDefinitionEntries_00587ed4;
extern CodeMotionEntryLink** gCodeMotionFPRUseEntries_00587ee8;
extern CodeMotionEntryLink** gCodeMotionFPRDefinitionEntries_00587f04;
extern CodeMotionEntryLink** gCodeMotionVRUseEntries_00587c88;
extern CodeMotionEntryLink** gCodeMotionVRDefinitionEntries_005876f0;

extern void* CodeMotion_Allocate(unsigned int size); /* 0x00441f20 */
extern void CodeMotion_FreeIteration(void);          /* 0x00441e20 */
extern void SpillCode_BuildBlockOrder(void);         /* 0x0049ce40 */
extern unsigned char COpt_0048ad10(CompilerObject* object);

extern void COpt_005237f0(void);
extern void COpt_00523920(void);
extern void COpt_00523a50(void);
extern void COpt_00525200(CodeMotionNode* node);
extern int COpt_00525fc0(PCodeInstruction* instruction, CodeMotionNode* node,
                         unsigned int* available_definitions);
extern void COpt_00526230(PCodeInstruction* instruction, CodeMotionNode* node);
extern int COpt_00526500(unsigned char* definition, CodeMotionNode* node);
extern int COpt_005266e0(int definition_index, CodeMotionNode* node);
extern int COpt_00526b50(PCodeInstruction* instruction, CodeMotionNode* node);
extern int COpt_00526d80(PCodeInstruction* instruction, CodeMotionNode* node,
                         unsigned int* available_definitions, int arg_3,
                         int arg_4);
extern void CodeMotion_CopyBits(unsigned int* destination,
                                const unsigned int* source,
                                int bit_count); /* 0x00533ed0 */

static unsigned int* CodeMotion_AllocateBits(int bit_count)
{
    return CodeMotion_Allocate(((bit_count + 31) >> 5) * sizeof(unsigned int));
}

static int CodeMotion_TestBit(const unsigned int* bits, int index)
{
    return (bits[index >> 5] & (1U << (index & 31))) != 0;
}

static void CodeMotion_ClearBit(unsigned int* bits, int index)
{
    bits[index >> 5] &= ~(1U << (index & 31));
}

static void CodeMotion_SetBit(unsigned int* bits, int index)
{
    bits[index >> 5] |= 1U << (index & 31);
}

static void CodeMotion_LinkEntry(CodeMotionEntryLink** head, int entry_index)
{
    CodeMotionEntryLink* link = CodeMotion_Allocate(sizeof(*link));

    link->entry_index = entry_index;
    link->next = *head;
    *head = link;
}

static CodeMotionEntryLink**
CodeMotion_RegisterEntryHead(unsigned char kind, short reg, int is_definition)
{
    if (kind == 0) {
        return is_definition ? &gCodeMotionGPRDefinitionEntries_00587ed4[reg]
                             : &gCodeMotionGPRUseEntries_00587f14[reg];
    }
    if (kind == 9) {
        return is_definition ? &gCodeMotionVRDefinitionEntries_005876f0[reg]
                             : &gCodeMotionVRUseEntries_00587c88[reg];
    }
    return is_definition ? &gCodeMotionFPRDefinitionEntries_00587f04[reg]
                         : &gCodeMotionFPRUseEntries_00587ee8[reg];
}

static void CodeMotion_SetExplicitEntry(CodeMotionEntry* entry,
                                        PCodeInstruction* instruction,
                                        PCodeOperand* operand)
{
    entry->instruction = instruction;
    entry->kind = operand->kind;
    entry->value.reg = operand->value.reg;
}

static void CodeMotion_SetObjectEntry(CodeMotionEntry* entry,
                                      PCodeInstruction* instruction,
                                      CompilerObject* object, int is_implicit)
{
    entry->instruction = instruction;
    entry->kind = 5;
    entry->is_implicit = is_implicit;
    entry->value.object = object;
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

/* 0x005240b0; high-level equivalent; 8.50% comparable byte match. */
void COpt_005240b0(int include_implicit)
{
    PCodeBlock* block;
    PCodeInstruction* instruction;
    int index;

    gCodeMotionUseEntries_00587650 = CodeMotion_Allocate(
        gCodeMotionUseCount_00587e38 * sizeof(CodeMotionEntry));
    gCodeMotionDefinitionEntries_00587588 = CodeMotion_Allocate(
        gCodeMotionDefinitionCount_00587ebc * sizeof(CodeMotionEntry));
    gCodeMotionGPRUseEntries_00587f14 = CodeMotion_Allocate(
        gUsedVirtualRegistersGPR * sizeof(CodeMotionEntryLink*));
    gCodeMotionGPRDefinitionEntries_00587ed4 = CodeMotion_Allocate(
        gUsedVirtualRegistersGPR * sizeof(CodeMotionEntryLink*));
    for (index = 0; index < gUsedVirtualRegistersGPR; index++) {
        gCodeMotionGPRUseEntries_00587f14[index] = 0;
        gCodeMotionGPRDefinitionEntries_00587ed4[index] = 0;
    }
    gCodeMotionFPRUseEntries_00587ee8 = CodeMotion_Allocate(
        gUsedVirtualRegistersFPR * sizeof(CodeMotionEntryLink*));
    gCodeMotionFPRDefinitionEntries_00587f04 = CodeMotion_Allocate(
        gUsedVirtualRegistersFPR * sizeof(CodeMotionEntryLink*));
    for (index = 0; index < gUsedVirtualRegistersFPR; index++) {
        gCodeMotionFPRUseEntries_00587ee8[index] = 0;
        gCodeMotionFPRDefinitionEntries_00587f04[index] = 0;
    }
    gCodeMotionVRUseEntries_00587c88 = CodeMotion_Allocate(
        gUsedVirtualRegistersVR * sizeof(CodeMotionEntryLink*));
    gCodeMotionVRDefinitionEntries_005876f0 = CodeMotion_Allocate(
        gUsedVirtualRegistersVR * sizeof(CodeMotionEntryLink*));
    for (index = 0; index < gUsedVirtualRegistersVR; index++) {
        gCodeMotionVRUseEntries_00587c88[index] = 0;
        gCodeMotionVRDefinitionEntries_005876f0[index] = 0;
    }

    for (block = gPCodeBlocks; block != 0; block = block->next) {
        for (instruction = block->instructions; instruction != 0;
             instruction = instruction->next)
        {
            int use_index;
            int definition_index;
            int operand_index;

            if ((instruction->flags & PCodeInstruction_SkipCodeMotion) != 0 ||
                instruction->operand_count == 0)
            {
                continue;
            }
            use_index = instruction->first_use_index;
            definition_index = instruction->first_definition_index;
            for (operand_index = 0; operand_index < instruction->operand_count;
                 operand_index++)
            {
                PCodeOperand* operand = &instruction->operands[operand_index];

                if ((operand->kind == 0 || operand->kind == 1 ||
                     operand->kind == 9) &&
                    operand->value.reg >= 32)
                {
                    if ((operand->flags & PCodeOperand_Use) != 0) {
                        CodeMotion_SetExplicitEntry(
                            &gCodeMotionUseEntries_00587650[use_index],
                            instruction, operand);
                        CodeMotion_LinkEntry(
                            CodeMotion_RegisterEntryHead(
                                operand->kind, operand->value.reg, 0),
                            use_index);
                        use_index++;
                    }
                    if ((operand->flags & PCodeOperand_Definition) != 0) {
                        CodeMotion_SetExplicitEntry(
                            &gCodeMotionDefinitionEntries_00587588
                                [definition_index],
                            instruction, operand);
                        CodeMotion_LinkEntry(
                            CodeMotion_RegisterEntryHead(
                                operand->kind, operand->value.reg, 1),
                            definition_index);
                        definition_index++;
                    }
                }
            }

            if (include_implicit != 0) {
                unsigned int flags = instruction->flags;

                if ((flags & PCodeInstruction_ImplicitUse) != 0) {
                    if ((flags & PCodeInstruction_NullObjectMemory) == 0) {
                        CompilerObject* object =
                            instruction->operands[2].object;
                        CodeMotionObjectNode* object_node =
                            COpt_00524b90(object);

                        CodeMotion_SetObjectEntry(
                            &gCodeMotionUseEntries_00587650[use_index],
                            instruction, object, 0);
                        CodeMotion_LinkEntry(&object_node->use_entries,
                                             use_index++);
                    } else {
                        CodeMotionObjectNode* object_node;
                        for (object_node = gCodeMotionAllocationList_005870fc;
                             object_node != 0;
                             object_node = object_node->allocation_next)
                        {
                            if (COpt_005248c0(instruction,
                                              object_node->object))
                            {
                                CodeMotion_SetObjectEntry(
                                    &gCodeMotionUseEntries_00587650[use_index],
                                    instruction, object_node->object, 1);
                                CodeMotion_LinkEntry(&object_node->use_entries,
                                                     use_index++);
                            }
                        }
                    }
                } else if ((flags & PCodeInstruction_ImplicitDefinition) != 0)
                {
                    if ((flags & PCodeInstruction_NullObjectMemory) == 0) {
                        CompilerObject* object =
                            instruction->operands[2].object;
                        CodeMotionObjectNode* object_node =
                            COpt_00524b90(object);

                        CodeMotion_SetObjectEntry(
                            &gCodeMotionDefinitionEntries_00587588
                                [definition_index],
                            instruction, object, 0);
                        CodeMotion_LinkEntry(&object_node->definition_entries,
                                             definition_index++);
                    } else {
                        CodeMotionObjectNode* object_node;
                        for (object_node = gCodeMotionAllocationList_005870fc;
                             object_node != 0;
                             object_node = object_node->allocation_next)
                        {
                            if (COpt_005248c0(instruction,
                                              object_node->object))
                            {
                                CodeMotion_SetObjectEntry(
                                    &gCodeMotionDefinitionEntries_00587588
                                        [definition_index],
                                    instruction, object_node->object, 1);
                                CodeMotion_LinkEntry(
                                    &object_node->definition_entries,
                                    definition_index++);
                            }
                        }
                    }
                } else if ((flags & PCodeInstruction_GPRFixedRange) != 0) {
                    CodeMotionObjectNode* object_node;
                    for (object_node = gCodeMotionAllocationList_005870fc;
                         object_node != 0;
                         object_node = object_node->allocation_next)
                    {
                        CompilerObject* object = object_node->object;
                        CompilerType* type = object->type;

                        if (object->kind == 0 || COpt_0048ad10(object) != 0 ||
                            (object->kind == 1 &&
                             (object->register_info_26->flags_22 != 0 ||
                              type->kind == 0x0c || type->kind == 0x04 ||
                              type->kind == 0x05 ||
                              (type->kind == 0x0a && type->size == 0x0c))))
                        {
                            CodeMotion_SetObjectEntry(
                                &gCodeMotionUseEntries_00587650[use_index],
                                instruction, object, 1);
                            CodeMotion_LinkEntry(&object_node->use_entries,
                                                 use_index++);
                            CodeMotion_SetObjectEntry(
                                &gCodeMotionDefinitionEntries_00587588
                                    [definition_index],
                                instruction, object, 1);
                            CodeMotion_LinkEntry(
                                &object_node->definition_entries,
                                definition_index++);
                        }
                    }
                }
            }
        }
    }
}

/* 0x005246d0; high-level equivalent; 12.62% comparable byte match. */
void COpt_005246d0(int include_implicit)
{
    PCodeBlock* block;
    PCodeInstruction* instruction;

    gCodeMotionDefinitionCount_00587ebc = 0;
    gCodeMotionUseCount_00587e38 = 0;
    for (block = gPCodeBlocks; block != 0; block = block->next) {
        for (instruction = block->instructions; instruction != 0;
             instruction = instruction->next)
        {
            int index;

            if ((instruction->flags & PCodeInstruction_SkipCodeMotion) != 0 ||
                instruction->operand_count == 0)
            {
                continue;
            }
            instruction->first_use_index = gCodeMotionUseCount_00587e38;
            instruction->first_definition_index =
                gCodeMotionDefinitionCount_00587ebc;
            for (index = 0; index < instruction->operand_count; index++) {
                PCodeOperand* operand = &instruction->operands[index];

                if ((operand->kind == 0 || operand->kind == 1 ||
                     operand->kind == 9) &&
                    operand->value.reg >= 32)
                {
                    if ((operand->flags & PCodeOperand_Use) != 0) {
                        gCodeMotionUseCount_00587e38++;
                    }
                    if ((operand->flags & PCodeOperand_Definition) != 0) {
                        gCodeMotionDefinitionCount_00587ebc++;
                    }
                }
            }

            if (include_implicit != 0) {
                if ((instruction->flags & PCodeInstruction_ImplicitUse) != 0) {
                    if ((instruction->flags &
                         PCodeInstruction_NullObjectMemory) == 0)
                    {
                        gCodeMotionUseCount_00587e38++;
                    } else {
                        CodeMotionObjectNode* object_node;

                        for (object_node = gCodeMotionAllocationList_005870fc;
                             object_node != 0;
                             object_node = object_node->allocation_next)
                        {
                            if (COpt_005248c0(instruction,
                                              object_node->object))
                            {
                                gCodeMotionUseCount_00587e38++;
                            }
                        }
                    }
                } else if ((instruction->flags &
                            PCodeInstruction_ImplicitDefinition) != 0)
                {
                    if ((instruction->flags &
                         PCodeInstruction_NullObjectMemory) == 0)
                    {
                        gCodeMotionDefinitionCount_00587ebc++;
                    } else {
                        CodeMotionObjectNode* object_node;

                        for (object_node = gCodeMotionAllocationList_005870fc;
                             object_node != 0;
                             object_node = object_node->allocation_next)
                        {
                            if (COpt_005248c0(instruction,
                                              object_node->object))
                            {
                                gCodeMotionDefinitionCount_00587ebc++;
                            }
                        }
                    }
                } else if ((instruction->flags &
                            PCodeInstruction_GPRFixedRange) != 0)
                {
                    CodeMotionObjectNode* object_node;

                    for (object_node = gCodeMotionAllocationList_005870fc;
                         object_node != 0;
                         object_node = object_node->allocation_next)
                    {
                        CompilerObject* object = object_node->object;
                        CompilerType* type = object->type;

                        if (object->kind == 0 || COpt_0048ad10(object) != 0 ||
                            (object->kind == 1 &&
                             (object->register_info_26->flags_22 != 0 ||
                              type->kind == 0x0c || type->kind == 0x04 ||
                              type->kind == 0x05 ||
                              (type->kind == 0x0a && type->size == 0x0c))))
                        {
                            gCodeMotionUseCount_00587e38++;
                            gCodeMotionDefinitionCount_00587ebc++;
                        }
                    }
                }
            }
        }
    }
}

/* 0x005248c0; high-level equivalent; 11.92% comparable byte match. */
int COpt_005248c0(PCodeInstruction* instruction, CompilerObject* object)
{
    CompilerType* type = object->type;

    if (object->kind != 0 && COpt_0048ad10(object) == 0) {
        if (object->kind != 1) {
            return 0;
        }
        if (object->register_info_26->flags_22 == 0 && type->kind != 0x0c &&
            type->kind != 0x04 && type->kind != 0x05 &&
            (type->kind != 0x0a || type->size != 0x0c))
        {
            return 0;
        }
    }

    switch (instruction->opcode) {
    case 0x8e:
    case 0x8f:
    case 0x90:
    case 0x91:
    case 0x96:
    case 0x97:
    case 0x98:
    case 0x99:
        while (type->kind == 0x0c) {
            type = type->wrapped_type;
        }
        if (type->kind == 2 && type->size == 4) {
            return 1;
        }
        return (type->kind == 4 || type->kind == 5) && type->size >= 4;

    case 0x92:
    case 0x93:
    case 0x94:
    case 0x95:
    case 0x9a:
    case 0x9b:
    case 0x9c:
    case 0x9d:
        while (type->kind == 0x0c) {
            type = type->wrapped_type;
        }
        if (type->kind == 2 && type->size != 4) {
            return 1;
        }
        return (type->kind == 4 || type->kind == 5) && type->size >= 8;

    case 0xf7:
    case 0xfc:
        while (type->kind == 0x0c) {
            type = type->wrapped_type;
        }
        if (type->kind == 4 && type->subtype >= 4 && type->subtype <= 14) {
            return 1;
        }
        if (type->kind == 4) {
            return type->value_10 == 0x10;
        }
        if (type->kind == 5) {
            return type->value_2e == 0x10;
        }
        return 0;

    case 0x22:
    case 0x23:
    case 0x24:
    case 0x25:
    case 0x31:
    case 0x32:
    case 0x33:
    case 0x34:
        if (type->kind == 0x0c || type->kind == 0x04 || type->kind == 0x05 ||
            (type->kind == 0x0a && type->size == 0x0c))
        {
            return 1;
        }
        if (type->kind == 2) {
            return 0;
        }
        return type->size == 4;

    case 0x19:
    case 0x1a:
    case 0x1b:
    case 0x1c:
    case 0x1d:
    case 0x1e:
    case 0x1f:
    case 0x20:
    case 0x2c:
    case 0x2d:
    case 0x2e:
    case 0x2f:
        if ((type->kind == 0x0c || type->kind == 0x04 || type->kind == 0x05 ||
             (type->kind == 0x0a && type->size == 0x0c)) &&
            (type->size & 2) != 0)
        {
            return 1;
        }
        return type->size == 2;

    default:
        return 1;
    }
}

/* 0x00524b20; control-flow equivalent; 26.04% comparable byte match. */
void COpt_00524b20(CompilerObject* object)
{
    CodeMotionObjectNode** link = &gCodeMotionObjectTree_005880ac;
    CodeMotionObjectNode* node;

    while ((node = *link) != 0) {
        if (object < node->object) {
            link = &node->left;
        } else if (object > node->object) {
            link = &node->right;
        } else {
            return;
        }
    }

    node = CodeMotion_Allocate(sizeof(CodeMotionObjectNode));
    node->right = 0;
    node->left = node->right;
    node->object = object;
    node->definition_entries = 0;
    node->use_entries = node->definition_entries;
    node->allocation_next = gCodeMotionAllocationList_005870fc;
    gCodeMotionAllocationList_005870fc = node;
    *link = node;
}

/* 0x00524b90; instruction-exact; 100.00% comparable byte match. */
#ifdef __MWERKS__
#pragma dont_inline on
#endif
CodeMotionObjectNode* COpt_00524b90(CompilerObject* object)
{
    CodeMotionObjectNode* node = gCodeMotionObjectTree_005880ac;

    while (node != 0) {
        if (object < node->object) {
            node = node->left;
        } else if (object > node->object) {
            node = node->right;
        } else {
            return node;
        }
    }
    return 0;
}
#ifdef __MWERKS__
#pragma dont_inline reset
#endif

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

/* 0x00524d90; high-level equivalent; 9.19% comparable byte match. */
void COpt_00524d90(CodeMotionNode* node)
{
    unsigned int* available_definitions;
    PCodeBlockLink* block_link;
    int changed;

    available_definitions =
        CodeMotion_AllocateBits(gCodeMotionDefinitionCount_00587ebc);
    do {
        changed = 0;
        for (block_link = node->blocks; block_link != 0;
             block_link = block_link->next)
        {
            PCodeBlock* block = block_link->block;
            PCodeInstruction* instruction;

            CodeMotion_CopyBits(available_definitions,
                                gCodeMotionBlockState_00587fe4[block->index]
                                    .definition_sets[2],
                                gCodeMotionDefinitionCount_00587ebc);
            for (instruction = block->instructions; instruction != 0;) {
                PCodeInstruction* next_instruction = instruction->next;

                if ((instruction->flags & PCodeInstruction_SkipCodeMotion) ==
                        0 &&
                    instruction->operand_count != 0)
                {
                    int can_move = 0;

                    if ((instruction->flags & 0x00020460) == 0 &&
                        COpt_00526d80(instruction, node, available_definitions,
                                      0, 0) != 0 &&
                        COpt_00526b50(instruction, node) != 0 &&
                        COpt_005266e0(instruction->first_definition_index,
                                      node) != 0 &&
                        (CodeMotion_TestBit(node->block_membership,
                                            instruction->block->index) ||
                         COpt_00526500(
                             &gCodeMotionDefinitionEntries_00587588
                                  [instruction->first_definition_index]
                                      .kind,
                             node) == 0) &&
                        (instruction->opcode != 0x89 ||
                         node->instruction_count <= 0x19))
                    {
                        can_move = 1;
                    }

                    if (can_move || COpt_00525fc0(instruction, node,
                                                  available_definitions) != 0)
                    {
                        COpt_00526230(instruction, node);
                        changed = 1;
                    }

                    {
                        int definition_index =
                            instruction->first_definition_index;
                        CodeMotionEntry* entry =
                            &gCodeMotionDefinitionEntries_00587588
                                [definition_index];

                        while (definition_index <
                                   gCodeMotionDefinitionCount_00587ebc &&
                               entry->instruction == instruction)
                        {
                            CodeMotionEntryLink* reverse_entries;

                            if (entry->kind == 0) {
                                reverse_entries =
                                    gCodeMotionGPRDefinitionEntries_00587ed4
                                        [entry->value.reg];
                                for (; reverse_entries != 0;
                                     reverse_entries = reverse_entries->next)
                                {
                                    CodeMotion_ClearBit(
                                        available_definitions,
                                        reverse_entries->entry_index);
                                }
                            } else if (entry->kind == 1) {
                                reverse_entries =
                                    gCodeMotionFPRDefinitionEntries_00587f04
                                        [entry->value.reg];
                                for (; reverse_entries != 0;
                                     reverse_entries = reverse_entries->next)
                                {
                                    CodeMotion_ClearBit(
                                        available_definitions,
                                        reverse_entries->entry_index);
                                }
                            } else if (entry->kind == 9) {
                                reverse_entries =
                                    gCodeMotionVRDefinitionEntries_005876f0
                                        [entry->value.reg];
                                for (; reverse_entries != 0;
                                     reverse_entries = reverse_entries->next)
                                {
                                    CodeMotion_ClearBit(
                                        available_definitions,
                                        reverse_entries->entry_index);
                                }
                            } else if (entry->is_implicit == 0) {
                                reverse_entries =
                                    COpt_00524b90(entry->value.object)
                                        ->definition_entries;
                                for (; reverse_entries != 0;
                                     reverse_entries = reverse_entries->next)
                                {
                                    CodeMotion_ClearBit(
                                        available_definitions,
                                        reverse_entries->entry_index);
                                }
                            }
                            CodeMotion_SetBit(available_definitions,
                                              definition_index);
                            definition_index++;
                            entry++;
                        }
                    }
                }
                instruction = next_instruction;
            }
        }
    } while (changed);
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
