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
extern InterferenceNode** gInterferenceGraph;   /* 0x00587e3c */
extern PCodeBlock* gPCodeBlocks;                /* 0x00587c74 */
extern PCodeBlockLiveness* gPCodeBlockLiveness; /* 0x00587e74 */
extern unsigned int* gInterferenceBits;         /* 0x00583088 */
extern short* gCoalescedRegisters;              /* 0x0058308c */

extern void SpillCode_005301b0(PCodeFunction* function, int reg_class,
                               int register_count);
extern void SpillCode_00531290(int reg_class, int register_count);
extern void SpillCode_00530e00(int reg_class, int register_count);
extern void SpillCode_DumpInterference(const char* format,
                                       int register_count); /* 0x004c4bc0 */
extern void* SpillCode_Allocate(unsigned int size);         /* 0x00441f20 */
extern int
SpillCode_HandleSpecialInstruction(PCodeInstruction* instruction,
                                   int reg_class,
                                   unsigned int* live); /* 0x00530050 */
extern void SpillCode_CopyLiveSet(unsigned int* destination,
                                  const void* source,
                                  int register_count); /* 0x00533ed0 */
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
    SpillCode_005301b0(function, reg_class, register_count);
    SpillCode_MarkLastUses(reg_class, register_count);
    SpillCode_00531290(reg_class, register_count);
    if (gCOptimizerDumpEnabled) {
        SpillCode_DumpInterference(SpillCode_RegisterFormat(reg_class),
                                   register_count);
    }
    SpillCode_00530e00(reg_class, register_count);
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

/* 0x00530a80; high-level equivalent; binary match unmeasured. */
void SpillCode_MarkLastUses(int reg_class, int register_count)
{
    unsigned int* live;
    PCodeBlock* block;

    live = SpillCode_Allocate(
        (unsigned int) (((register_count + 31) >> 5) * sizeof(*live)));
    for (block = gPCodeBlocks; block != 0; block = block->next) {
        PCodeInstruction* instruction;

        SpillCode_CopyLiveSet(live, gPCodeBlockLiveness[block->index].live_out,
                              register_count);
        for (instruction = block->reverse_instructions; instruction != 0;
             instruction = instruction->previous)
        {
            int index;

            if (SpillCode_HandleSpecialInstruction(instruction, reg_class,
                                                   live))
            {
                PCode_RemoveRedundantInstruction(instruction);
                continue;
            }

            for (index = 0; index < instruction->operand_count; index++) {
                PCodeOperand* operand;

                operand = &instruction->operands[index];
                if (operand->kind == reg_class &&
                    (operand->flags & PCodeOperand_Definition) != 0)
                {
                    SpillCode_ClearLive(live, operand->reg);
                }
            }
            for (index = 0; index < instruction->operand_count; index++) {
                PCodeOperand* operand;

                operand = &instruction->operands[index];
                if (operand->kind == reg_class &&
                    (operand->flags & PCodeOperand_Use) != 0)
                {
                    if (!SpillCode_IsLive(live, operand->reg)) {
                        operand->flags |= PCodeOperand_LastUse;
                    }
                    SpillCode_SetLive(live, operand->reg);
                }
            }
        }
    }
}

static int SpillCode_Interferes(int first, int second)
{
    unsigned int index;
    int larger;
    int smaller;

    if (first == second) {
        return 0;
    }
    if (first > second) {
        larger = first;
        smaller = second;
    } else {
        larger = second;
        smaller = first;
    }
    index = (unsigned int) ((larger * larger) / 2 + smaller);
    return (gInterferenceBits[index >> 5] & (1U << (index & 31))) != 0;
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
            gInterferenceGraph[operand->reg]->spill_cost +=
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
