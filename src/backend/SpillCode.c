/*
 * SpillCode.c
 *
 * Direct source-file anchors identify six initial functions at
 * 0x00531ab0-0x00532774. Recover these with the Coloring.c retry paths so the
 * distinction between spill selection and spill-code insertion stays clear.
 */

#include "mwcc/SpillCode.h"

#include "mwcc/Coloring.h"

extern unsigned char gCOptimizerDumpEnabled;    /* 0x00584226 */
extern unsigned char gUniformSpillBlockWeight;  /* 0x005842e2 */
extern signed char gDeleteDeadInstructions;     /* 0x005842e1 */
extern InterferenceNode** gInterferenceGraph;   /* 0x00587e3c */
extern PCodeBlock* gPCodeBlocks;                /* 0x00587c74 */
extern PCodeBlockLiveness* gPCodeBlockLiveness; /* 0x00587e74 */
extern PCodeBlock** gPCodeBlockOrder;           /* 0x00587fbc */
extern int gPCodeBlockCount;                    /* 0x00587190 */
extern PCodeBlock* gReturnBlock;                /* 0x00587ec8 */
extern PCodeBlock* gCurrentBlock;               /* 0x005880c4 */
extern unsigned int* gInterferenceBits;         /* 0x00583088 */
extern short* gCoalescedRegisters;              /* 0x0058308c */
extern short gGPRCoalesceFirst;                 /* 0x005882da */
extern short gGPRCoalesceLast;                  /* 0x005882e2 */
extern short gFPRCoalesceFirst;                 /* 0x005882dc */
extern short gFPRCoalesceLast;                  /* 0x005882e0 */
extern short gVRCoalesceFirst;                  /* 0x00588464 */
extern short gVRCoalesceLast;                   /* 0x0058846a */
extern unsigned char gUseGPRForType2Return;     /* 0x00584244 */

extern void SpillCode_BuildBlockOrder(void);              /* 0x0049ce40 */
extern int Type_RequiresMemoryReturn(CompilerType* type); /* 0x004a7af0 */
extern void SpillCode_DumpInterference(const char* format,
                                       int register_count); /* 0x004c4bc0 */
extern void* SpillCode_Allocate(unsigned int size);         /* 0x00441f20 */
extern void PCode_RemoveRedundantInstruction(PCodeInstruction* instruction);

static const char* SpillCode_RegisterFormat(int reg_class)
{
    if (reg_class == 0) {
        return " r%ld";
    }
    if (reg_class == 1) {
        return " f%ld";
    }
    return " vr%ld";
}

/* 0x00530a00; control-flow equivalent; binary match unmeasured. */
void SpillCode_BuildInterference(PCodeFunction* function, int reg_class,
                                 int register_count)
{
    SpillCode_InitializeLiveness(function, reg_class, register_count);
    SpillCode_MarkLastUses(reg_class, register_count);
    SpillCode_ConstructInterference(reg_class, register_count);
    if (gCOptimizerDumpEnabled) {
        SpillCode_DumpInterference(SpillCode_RegisterFormat(reg_class),
                                   register_count);
    }
    SpillCode_CoalesceCopies(reg_class, register_count);
    SpillCode_MaterializeGraph(register_count);
}

static void SpillCode_ClearLive(unsigned int* live, short reg)
{
    live[reg >> 5] &= ~(1U << (reg & 31));
}

static int SpillCode_IsLive(unsigned int* live, short reg)
{
    return (live[reg >> 5] & (1U << (reg & 31))) != 0;
}

static void SpillCode_SetLive(unsigned int* live, short reg)
{
    live[reg >> 5] |= 1U << (reg & 31);
}

static int SpillCode_WordCount(int register_count)
{
    return (register_count + 31) >> 5;
}

static void SpillCode_CopyBits(unsigned int* destination,
                               const unsigned int* source, int bit_count)
{
    int word;

    for (word = 0; word < SpillCode_WordCount(bit_count); word++) {
        destination[word] = source[word];
    }
}

static void SpillCode_OrBits(unsigned int* destination,
                             const unsigned int* source, int bit_count)
{
    int word;

    for (word = 0; word < SpillCode_WordCount(bit_count); word++) {
        destination[word] |= source[word];
    }
}

static unsigned int* SpillCode_AllocateEmptyBits(int register_count)
{
    unsigned int* bits;
    int word;

    bits = SpillCode_Allocate(
        (unsigned int) (SpillCode_WordCount(register_count) * sizeof(*bits)));
    for (word = 0; word < SpillCode_WordCount(register_count); word++) {
        bits[word] = 0;
    }
    return bits;
}

static void SpillCode_AddBlockUse(PCodeBlock* block, short reg)
{
    if (block != 0) {
        SpillCode_SetLive(gPCodeBlockLiveness[block->index].use, reg);
    }
}

static int SpillCode_IsDirectGPRScalar(const CompilerType* type)
{
    return type->kind == 1 || type->kind == 3 || type->kind == 11 ||
           (type->kind == 10 && type->size == 4);
}

static void SpillCode_SeedGPRReturn(CompilerType* type)
{
    if (SpillCode_IsDirectGPRScalar(type)) {
        SpillCode_AddBlockUse(gReturnBlock, 3);
        if ((type->kind == 1 || type->kind == 3) && type->size == 8) {
            SpillCode_AddBlockUse(gCurrentBlock, 4);
        }
    } else if ((type->kind == 4 || type->kind == 5) &&
               !Type_RequiresMemoryReturn(type))
    {
        SpillCode_AddBlockUse(gReturnBlock, 3);
        if (type->size > 4) {
            SpillCode_AddBlockUse(gCurrentBlock, 4);
        }
    } else if (gUseGPRForType2Return && type->kind == 2) {
        SpillCode_AddBlockUse(gReturnBlock, 3);
        if (type->size == 8) {
            SpillCode_AddBlockUse(gCurrentBlock, 4);
        }
    }
}

static void SpillCode_SeedReturnRegisters(CompilerType* type, int reg_class)
{
    if (reg_class == 0) {
        SpillCode_SeedGPRReturn(type);
    } else if (reg_class == 1 && type->kind == 2) {
        SpillCode_AddBlockUse(gReturnBlock, 1);
    } else if (reg_class == 9 && type->kind == 4 && type->subtype >= 4 &&
               type->subtype <= 14)
    {
        SpillCode_AddBlockUse(gReturnBlock, 2);
    }
}

static int SpillCode_DefinitionBlocksRemoval(PCodeOperand* operand,
                                             int reg_class, unsigned int* live)
{
    if ((operand->flags & PCodeOperand_Definition) == 0) {
        return 0;
    }
    if (operand->kind == 0) {
        return reg_class != 0 || SpillCode_IsLive(live, operand->value.reg);
    }
    if (operand->kind == 1) {
        return reg_class != 1 || SpillCode_IsLive(live, operand->value.reg);
    }
    if (operand->kind == 9) {
        return reg_class != 9 || SpillCode_IsLive(live, operand->value.reg);
    }
    return operand->kind == 2 || operand->kind == 3;
}

/* 0x00530050; high-level equivalent; binary match unmeasured. */
int SpillCode_IsDeadInstruction(PCodeInstruction* instruction, int reg_class,
                                unsigned int* live)
{
    int index;

    if ((instruction->flags & PCodeInstruction_DeadCodeBarrierMask) != 0) {
        return 0;
    }
    if (instruction->context != 0 && (instruction->context->flags & 0x03) != 0)
    {
        return 0;
    }
    for (index = 0; index < instruction->operand_count; index++) {
        if (SpillCode_DefinitionBlocksRemoval(&instruction->operands[index],
                                              reg_class, live))
        {
            return 0;
        }
    }
    return gDeleteDeadInstructions > 0;
}

/* 0x00530a80; high-level equivalent; binary match unmeasured. */
void SpillCode_MarkLastUses(int reg_class, int register_count)
{
    unsigned int* live;
    PCodeBlock* block;

    live = SpillCode_Allocate(
        (unsigned int) (((register_count + 31) >> 5) * sizeof(*live)));
    for (block = gPCodeBlocks; block != 0; block = block->next) {
        PCodeInstruction* instruction;

        SpillCode_CopyBits(live, gPCodeBlockLiveness[block->index].live_out,
                           register_count);
        for (instruction = block->reverse_instructions; instruction != 0;
             instruction = instruction->previous)
        {
            int index;

            if (SpillCode_IsDeadInstruction(instruction, reg_class, live)) {
                PCode_RemoveRedundantInstruction(instruction);
                continue;
            }

            for (index = 0; index < instruction->operand_count; index++) {
                PCodeOperand* operand;

                operand = &instruction->operands[index];
                if (operand->kind == reg_class &&
                    (operand->flags & PCodeOperand_Definition) != 0)
                {
                    SpillCode_ClearLive(live, operand->value.reg);
                }
            }
            for (index = 0; index < instruction->operand_count; index++) {
                PCodeOperand* operand;

                operand = &instruction->operands[index];
                if (operand->kind == reg_class &&
                    (operand->flags & PCodeOperand_Use) != 0)
                {
                    if (!SpillCode_IsLive(live, operand->value.reg)) {
                        operand->flags |= PCodeOperand_LastUse;
                    }
                    SpillCode_SetLive(live, operand->value.reg);
                }
            }
        }
    }
}

static unsigned int SpillCode_MatrixIndex(int first, int second)
{
    unsigned int index;
    int larger;
    int smaller;

    if (first == second) {
        return (unsigned int) ((first * first) / 2);
    }
    if (first > second) {
        larger = first;
        smaller = second;
    } else {
        larger = second;
        smaller = first;
    }
    index = (unsigned int) ((larger * larger) / 2 + smaller);
    return index;
}

static int SpillCode_Interferes(int first, int second)
{
    unsigned int index;

    if (first == second) {
        return 0;
    }
    index = SpillCode_MatrixIndex(first, second);
    return (gInterferenceBits[index >> 5] & (1U << (index & 31))) != 0;
}

static void SpillCode_SetMatrixBit(int first, int second)
{
    unsigned int index;

    index = SpillCode_MatrixIndex(first, second);
    gInterferenceBits[index >> 5] |= 1U << (index & 31);
}

static void SpillCode_SetInterference(int first, int second)
{
    if (first != second) {
        SpillCode_SetMatrixBit(first, second);
    }
}

static void SpillCode_ClearWords(unsigned int* bits, int bit_count)
{
    int word;
    int word_count;

    word_count = SpillCode_WordCount(bit_count);
    for (word = 0; word < word_count; word++) {
        bits[word] = 0;
    }
}

static void SpillCode_PrecolorPhysicalRegisters(void)
{
    int first;
    int second;

    for (first = 0; first < 32; first++) {
        for (second = 0; second < 32; second++) {
            SpillCode_SetInterference(first, second);
        }
    }
}

static int SpillCode_CopySourceExcluded(PCodeInstruction* instruction, int reg)
{
    return (instruction->flags & PCodeInstruction_CopySourceExclusion) != 0 &&
           instruction->operands[1].value.reg == reg;
}

static void SpillCode_AddDefinitionEdges(PCodeInstruction* instruction,
                                         int reg_class, unsigned int* live,
                                         int register_count)
{
    int index;

    for (index = 0; index < instruction->operand_count; index++) {
        PCodeOperand* operand;
        int other;

        operand = &instruction->operands[index];
        if (operand->kind != reg_class ||
            (operand->flags & PCodeOperand_Definition) == 0)
        {
            continue;
        }

        SpillCode_ClearLive(live, operand->value.reg);
        for (other = 0; other < register_count; other++) {
            if (SpillCode_IsLive(live, (short) other) &&
                !SpillCode_CopySourceExcluded(instruction, other))
            {
                SpillCode_SetInterference(operand->value.reg, other);
            }
        }
    }
}

static void SpillCode_AddUses(PCodeInstruction* instruction, int reg_class,
                              unsigned int* live)
{
    int index;

    for (index = 0; index < instruction->operand_count; index++) {
        PCodeOperand* operand;

        operand = &instruction->operands[index];
        if (operand->kind == reg_class &&
            (operand->flags & PCodeOperand_Use) != 0)
        {
            if (!SpillCode_IsLive(live, operand->value.reg)) {
                operand->flags |= PCodeOperand_LastUse;
            }
            SpillCode_SetLive(live, operand->value.reg);
        }
    }
}

static void SpillCode_MarkConstrainedRegister(short reg)
{
    if (reg >= 32) {
        SpillCode_SetMatrixBit(reg, reg);
    }
}

static void SpillCode_AddGPRConstraints(PCodeInstruction* instruction)
{
    if ((instruction->flags & PCodeInstruction_GPRResultMask) != 0) {
        SpillCode_MarkConstrainedRegister(instruction->operands[1].value.reg);
        if ((instruction->flags & PCodeInstruction_GPRPairInterference) != 0) {
            SpillCode_SetInterference(instruction->operands[0].value.reg,
                                      instruction->operands[1].value.reg);
        }
    } else if (instruction->opcode == 0x3f || instruction->opcode == 0x42) {
        SpillCode_MarkConstrainedRegister(instruction->operands[1].value.reg);
    } else if (instruction->opcode >= 0x37 && instruction->opcode <= 0x3b) {
        SpillCode_MarkConstrainedRegister(instruction->operands[0].value.reg);
    }

    if ((instruction->flags & PCodeInstruction_GPRFixedRange) != 0) {
        int index;

        for (index = 50; index < instruction->operand_count; index++) {
            int physical;
            short reg;

            reg = instruction->operands[index].value.reg;
            SpillCode_MarkConstrainedRegister(reg);
            for (physical = 3; physical <= 12; physical++) {
                SpillCode_SetInterference(reg, physical);
            }
        }
    }
}

/* 0x00531290; high-level equivalent; binary match unmeasured. */
void SpillCode_ConstructInterference(int reg_class, int register_count)
{
    unsigned int* live;
    int matrix_bit_count;
    PCodeBlock* block;

    matrix_bit_count = (register_count * register_count) / 2;
    gInterferenceBits =
        SpillCode_Allocate((unsigned int) (((matrix_bit_count + 31) >> 5) *
                                           sizeof(*gInterferenceBits)));
    SpillCode_ClearWords(gInterferenceBits, matrix_bit_count);
    SpillCode_PrecolorPhysicalRegisters();

    live = SpillCode_Allocate(
        (unsigned int) (((register_count + 31) >> 5) * sizeof(*live)));
    for (block = gPCodeBlocks; block != 0; block = block->next) {
        PCodeInstruction* instruction;

        SpillCode_CopyBits(live, gPCodeBlockLiveness[block->index].live_out,
                           register_count);
        for (instruction = block->reverse_instructions; instruction != 0;
             instruction = instruction->previous)
        {
            SpillCode_AddDefinitionEdges(instruction, reg_class, live,
                                         register_count);
            SpillCode_AddUses(instruction, reg_class, live);
            if (reg_class == 0) {
                SpillCode_AddGPRConstraints(instruction);
            }
        }
    }
}

/* 0x00530530; high-level equivalent; binary match unmeasured. */
void SpillCode_BuildLocalLiveness(int reg_class)
{
    PCodeBlock* block;

    for (block = gPCodeBlocks; block != 0; block = block->next) {
        PCodeBlockLiveness* liveness;
        PCodeInstruction* instruction;

        liveness = &gPCodeBlockLiveness[block->index];
        for (instruction = block->instructions; instruction != 0;
             instruction = instruction->next)
        {
            int index;

            for (index = 0; index < instruction->operand_count; index++) {
                PCodeOperand* operand;

                operand = &instruction->operands[index];
                if (operand->kind == reg_class &&
                    (operand->flags & PCodeOperand_Use) != 0 &&
                    !SpillCode_IsLive(liveness->def, operand->value.reg))
                {
                    SpillCode_SetLive(liveness->use, operand->value.reg);
                }
            }
            for (index = 0; index < instruction->operand_count; index++) {
                PCodeOperand* operand;

                operand = &instruction->operands[index];
                if (operand->kind == reg_class &&
                    (operand->flags & PCodeOperand_Definition) != 0 &&
                    !SpillCode_IsLive(liveness->use, operand->value.reg))
                {
                    SpillCode_SetLive(liveness->def, operand->value.reg);
                }
            }
        }
    }
}

static void SpillCode_CollectSuccessorLiveIn(PCodeBlock* block,
                                             unsigned int* live_out,
                                             int register_count)
{
    PCodeBlockLink* successor;

    successor = block->successors;
    if (successor == 0) {
        return;
    }
    SpillCode_CopyBits(live_out,
                       gPCodeBlockLiveness[successor->block->index].live_in,
                       register_count);
    for (successor = successor->next; successor != 0;
         successor = successor->next)
    {
        SpillCode_OrBits(live_out,
                         gPCodeBlockLiveness[successor->block->index].live_in,
                         register_count);
    }
}

static int SpillCode_UpdateLiveIn(PCodeBlockLiveness* liveness,
                                  int register_count)
{
    int changed;
    int word;

    changed = 0;
    for (word = 0; word < SpillCode_WordCount(register_count); word++) {
        unsigned int live_in;

        live_in = (~liveness->def[word] & liveness->live_out[word]) |
                  liveness->use[word];
        if (live_in != liveness->live_in[word]) {
            liveness->live_in[word] = live_in;
            changed = 1;
        }
    }
    return changed;
}

/* 0x00530410; high-level equivalent; binary match unmeasured. */
void SpillCode_SolveLiveness(int register_count)
{
    int changed;

    do {
        int order_index;

        changed = 0;
        for (order_index = gPCodeBlockCount - 1; order_index >= 0;
             order_index--)
        {
            PCodeBlock* block;
            PCodeBlockLiveness* liveness;

            block = gPCodeBlockOrder[order_index];
            if (block == 0) {
                continue;
            }
            liveness = &gPCodeBlockLiveness[block->index];
            SpillCode_CollectSuccessorLiveIn(block, liveness->live_out,
                                             register_count);
            if (SpillCode_UpdateLiveIn(liveness, register_count)) {
                changed = 1;
            }
        }
    } while (changed);
}

/* 0x005301b0; high-level equivalent; binary match unmeasured. */
void SpillCode_InitializeLiveness(PCodeFunction* function, int reg_class,
                                  int register_count)
{
    CompilerType* result_type;
    int block_index;

    result_type = function->signature->result_type;
    SpillCode_BuildBlockOrder();
    gPCodeBlockLiveness = SpillCode_Allocate(
        (unsigned int) (gPCodeBlockCount * sizeof(*gPCodeBlockLiveness)));
    for (block_index = 0; block_index < gPCodeBlockCount; block_index++) {
        PCodeBlockLiveness* liveness;

        liveness = &gPCodeBlockLiveness[block_index];
        liveness->use = SpillCode_AllocateEmptyBits(register_count);
        liveness->def = SpillCode_AllocateEmptyBits(register_count);
        liveness->live_in = SpillCode_AllocateEmptyBits(register_count);
        liveness->live_out = SpillCode_AllocateEmptyBits(register_count);
    }

    SpillCode_BuildLocalLiveness(reg_class);
    SpillCode_SeedReturnRegisters(result_type, reg_class);
    SpillCode_SolveLiveness(register_count);
}

static short SpillCode_CoalesceRoot(short reg)
{
    short parent;

    do {
        parent = gCoalescedRegisters[reg];
        if (parent == reg) {
            return reg;
        }
        reg = parent;
    } while (1);
}

static short SpillCode_CopyOpcode(int reg_class)
{
    if (reg_class == 0) {
        return 0x8b;
    }
    if (reg_class == 1) {
        return 0x9e;
    }
    return 0x18e;
}

static int SpillCode_CoalesceEligible(int reg_class, short reg)
{
    if (reg_class == 0) {
        return reg >= gGPRCoalesceFirst && reg <= gGPRCoalesceLast;
    }
    if (reg_class == 1) {
        return reg >= gFPRCoalesceFirst && reg <= gFPRCoalesceLast;
    }
    return reg >= gVRCoalesceFirst && reg <= gVRCoalesceLast;
}

static int SpillCode_CanCoalesce(int reg_class, short first, short second)
{
    if (SpillCode_Interferes(first, second)) {
        return 0;
    }
    if (first < 32 || second < 32) {
        return 1;
    }
    return SpillCode_CoalesceEligible(reg_class, first) &&
           SpillCode_CoalesceEligible(reg_class, second);
}

static void SpillCode_MergeCoalesceRoots(short first, short second,
                                         int register_count)
{
    short root;
    short child;
    int reg;

    root = first < second ? first : second;
    child = first < second ? second : first;
    gCoalescedRegisters[child] = root;
    for (reg = 0; reg < register_count; reg++) {
        if (SpillCode_Interferes(child, reg)) {
            SpillCode_SetInterference(root, reg);
        }
    }
}

/* 0x00530e00; high-level equivalent; binary match unmeasured. */
void SpillCode_CoalesceCopies(int reg_class, int register_count)
{
    PCodeBlock* block;
    int reg;

    gCoalescedRegisters = SpillCode_Allocate(
        (unsigned int) (register_count * sizeof(*gCoalescedRegisters)));
    for (reg = 0; reg < register_count; reg++) {
        gCoalescedRegisters[reg] = (short) reg;
    }

    for (block = gPCodeBlocks; block != 0; block = block->next) {
        PCodeInstruction* instruction;

        for (instruction = block->instructions; instruction != 0;
             instruction = instruction->next)
        {
            if (instruction->opcode == SpillCode_CopyOpcode(reg_class) &&
                (instruction->flags & PCodeInstruction_CoalesceDisabled) == 0)
            {
                short first;
                short second;

                first =
                    SpillCode_CoalesceRoot(instruction->operands[0].value.reg);
                second =
                    SpillCode_CoalesceRoot(instruction->operands[1].value.reg);
                if (first == second) {
                    PCode_RemoveRedundantInstruction(instruction);
                } else if (SpillCode_CanCoalesce(reg_class, first, second)) {
                    SpillCode_MergeCoalesceRoots(first, second,
                                                 register_count);
                    PCode_RemoveRedundantInstruction(instruction);
                }
            }
        }
    }

    for (block = gPCodeBlocks; block != 0; block = block->next) {
        PCodeInstruction* instruction;

        for (instruction = block->instructions; instruction != 0;
             instruction = instruction->next)
        {
            int index;

            for (index = 0; index < instruction->operand_count; index++) {
                PCodeOperand* operand;

                operand = &instruction->operands[index];
                if (operand->kind == reg_class) {
                    operand->value.reg =
                        SpillCode_CoalesceRoot(operand->value.reg);
                }
            }
        }
    }
}

/* 0x00530c00; high-level equivalent; binary match unmeasured. */
void SpillCode_MaterializeGraph(int register_count)
{
    short* neighbors;
    int reg;

    gInterferenceGraph = SpillCode_Allocate(
        (unsigned int) (register_count * sizeof(*gInterferenceGraph)));
    neighbors = SpillCode_Allocate(
        (unsigned int) (register_count * sizeof(*neighbors)));

    for (reg = 0; reg < register_count; reg++) {
        InterferenceNode* node;
        int neighbor_count;
        int other;
        unsigned int size;

        neighbor_count = 0;
        for (other = 0; other < register_count; other++) {
            if (SpillCode_Interferes(reg, other)) {
                neighbors[neighbor_count++] = (short) other;
            }
        }

        size = (unsigned int) (offsetof(InterferenceNode, neighbors) +
                               neighbor_count * sizeof(short));
        node = SpillCode_Allocate(size);
        gInterferenceGraph[reg] = node;
        node->next = 0;
        node->object = 0;
        node->spill_cost = 0;
        node->virtual_register = (short) reg;
        node->degree = (short) neighbor_count;
        node->physical_register = -1;
        node->flags = 0;
        node->neighbor_count = (short) neighbor_count;
        for (other = 0; other < neighbor_count; other++) {
            node->neighbors[other] = neighbors[other];
        }

        if (reg != gCoalescedRegisters[reg]) {
            short root;

            root = SpillCode_CoalesceRoot((short) reg);
            node->flags |= Interference_Coalesced;
            node->physical_register = root;
            gInterferenceGraph[root]->flags |= Interference_CoalesceTarget;
        }
    }
}

static void SpillCode_AddOperandCosts(PCodeInstruction* instruction,
                                      int reg_class, int block_weight,
                                      unsigned char flag, int multiplier)
{
    int index;

    for (index = 0; index < instruction->operand_count; index++) {
        PCodeOperand* operand;

        operand = &instruction->operands[index];
        if (operand->kind == reg_class && (operand->flags & flag) != 0) {
            gInterferenceGraph[operand->value.reg]->spill_cost +=
                block_weight * multiplier;
        }
    }
}

/* 0x00532790; high-level equivalent; binary match unmeasured. */
void SpillCode_ComputeSpillCosts(int reg_class)
{
    PCodeBlock* block;

    for (block = gPCodeBlocks; block != 0; block = block->next) {
        PCodeInstruction* instruction;
        int block_weight;

        block_weight = gUniformSpillBlockWeight ? 1 : block->execution_weight;
        for (instruction = block->instructions; instruction != 0;
             instruction = instruction->next)
        {
            SpillCode_AddOperandCosts(instruction, reg_class, block_weight, 1,
                                      2);
            SpillCode_AddOperandCosts(instruction, reg_class, block_weight, 2,
                                      1);
        }
    }
}
