/*
 * SpillCode.c
 *
 * Direct source-file anchors identify six initial functions at
 * 0x00531ab0-0x00532774. Recover these with the Coloring.c retry paths so the
 * distinction between spill selection and spill-code insertion stays clear.
 */

#include "mwcc/SpillCode.h"

extern unsigned char gCOptimizerDumpEnabled;   /* 0x00584226 */
extern unsigned char gUniformSpillBlockWeight; /* 0x005842e2 */
extern InterferenceNode** gInterferenceGraph;  /* 0x00587e3c */
extern PCodeBlock* gPCodeBlocks;               /* 0x00587c74 */

extern void SpillCode_005301b0(PCodeFunction* function, int reg_class,
                               int register_count);
extern void SpillCode_00530a80(int reg_class, int register_count);
extern void SpillCode_00531290(int reg_class, int register_count);
extern void SpillCode_00530e00(int reg_class, int register_count);
extern void SpillCode_00530c00(int register_count);
extern void SpillCode_DumpInterference(const char* format,
                                       int register_count); /* 0x004c4bc0 */

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
    SpillCode_00530a80(reg_class, register_count);
    SpillCode_00531290(reg_class, register_count);
    if (gCOptimizerDumpEnabled) {
        SpillCode_DumpInterference(SpillCode_RegisterFormat(reg_class),
                                   register_count);
    }
    SpillCode_00530e00(reg_class, register_count);
    SpillCode_00530c00(register_count);
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
