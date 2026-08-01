/*
 * Coloring.c
 *
 * Working entry points:
 *   0x004cdef0  Coloring_AllocateRegisters
 *   0x004ce710  Coloring_SetupFPRs
 *
 * The coordinator handles vector, GPR, and FPR classes in that order. Each
 * class has an independent spill-and-retry loop once its virtual-register
 * namespace grows beyond the 32 physical registers.
 */

#include "mwcc/Coloring.h"
#include "mwcc/Registers.h"
#include "mwcc/SpillCode.h"

extern unsigned char gCOptimizerDumpEnabled;  /* 0x00584226 */
extern unsigned char gColoringGuard_00584244; /* role not yet established */
extern unsigned char gHasAltivecFrame;        /* 0x005884f9 */
extern short gUsedVirtualRegistersVR;         /* 0x0058849a */
extern short gUsedVirtualRegistersGPR;        /* 0x0058846e */
extern short gUsedVirtualRegistersFPR;        /* 0x0058846c */
extern short gColoringRegisterCount;          /* 0x00581b88 */
extern int gVirtualRegistersActive;           /* 0x00587648, inferred */

extern char* Coloring_GetFunctionObject(PCodeFunction* function);
extern void Coloring_Dump(const char* function_name, const char* stage);
extern void Coloring_Error(int code, const char* register_class);
extern void Coloring_Assert(const char* file, int line);

extern void Registers_SetupVRs(void);     /* 0x004c1560 */
extern void Registers_SetupGPRs(void);    /* 0x004c15c0 */
extern void Registers_SetupFPRs(void);    /* 0x004c1590 */
extern int Registers_AvailableVRs(void);  /* 0x004c1ae0 */
extern int Registers_AvailableGPRs(void); /* 0x004c1b20 */
extern int Registers_AvailableFPRs(void); /* 0x004c1b00 */

extern void Coloring_SetupVRs(void);  /* 0x004ce5f0 */
extern void Coloring_SetupGPRs(void); /* 0x004ce850 */
extern void Coloring_SetupFPRs(void); /* 0x004ce710 */
extern void SpillCode_00531800(int reg_class, int register_count);
extern void Coloring_FreeIteration(void);  /* 0x00441e20 */
extern void StackFrame_CheckAltivec(void); /* 0x004a9c80 */

extern void Coloring_ResetGPRColors(void);       /* 0x004c1530 */
extern void Coloring_ResetFPRColors(void);       /* 0x004c1500 */
extern void Coloring_ResetVRColors(void);        /* 0x004c14d0 */
extern unsigned int Coloring_GPRColorMask(void); /* 0x004c1ac0 */
extern unsigned int Coloring_FPRColorMask(void); /* 0x004c1aa0 */
extern unsigned int Coloring_VRColorMask(void);  /* 0x004c1a80 */
extern short Coloring_ClaimGPRColor(void);       /* 0x004c1a50 */
extern short Coloring_ClaimFPRColor(void);       /* 0x004c1a20 */
extern short Coloring_ClaimVRColor(void);        /* 0x004c19f0 */
extern void PCode_RemoveRedundantInstruction(PCodeInstruction* instruction);
/* 0x0049d010 */

extern InterferenceNode** gInterferenceGraph; /* 0x00587e3c */
extern ObjectList* gRegisterObjectList1;      /* 0x0058806c */
extern ObjectList* gRegisterObjectList2;      /* 0x00587fb8 */
extern PCodeBlock* gPCodeBlocks;              /* 0x00587c74 */
extern float gSpillScore_0056309c;
extern float gSpillScore_005630a0;
static void Coloring_RunClass(PCodeFunction* function, int reg_class,
                              int register_count,
                              int (*available_registers)(void),
                              void (*setup_class)(void))
{
    int retry;
    InterferenceNode* graph;

    retry = 1;
    while (retry && register_count > 32) {
        SpillCode_BuildInterference(function, reg_class, register_count);
        setup_class();
        retry = 0;
        graph = Coloring_SimplifyGraph(reg_class, available_registers(),
                                       register_count);
        if (!Coloring_SelectColors(reg_class, graph)) {
            retry = 1;
        }
        if (retry) {
            SpillCode_00531800(reg_class, register_count);
        } else {
            Coloring_CommitAssignments(reg_class, register_count);
        }
        Coloring_FreeIteration();
    }
}

static void Coloring_DecrementNeighbors(InterferenceNode* node)
{
    int index;

    for (index = 0; index < node->neighbor_count; index++) {
        gInterferenceGraph[node->neighbors[index]]->degree--;
    }
}

static int Coloring_SimplifyLowDegree(int available_colors, int register_count,
                                      InterferenceNode** stack,
                                      InterferenceNode** remaining)
{
    int changed;
    int reg;

    changed = 0;
    *remaining = 0;
    for (reg = 32; reg < register_count; reg++) {
        InterferenceNode* node;

        node = gInterferenceGraph[reg];
        if ((node->flags &
             (Interference_Simplified | Interference_Coalesced)) == 0)
        {
            if (node->degree < available_colors) {
                Coloring_DecrementNeighbors(node);
                node->flags |= Interference_Simplified;
                node->next = *stack;
                *stack = node;
                changed = 1;
            } else {
                node->next = *remaining;
                *remaining = node;
            }
        }
    }
    return changed;
}

static float Coloring_SpillScore(InterferenceNode* node, float fallback)
{
    if (node->virtual_register >= gColoringRegisterCount) {
        return fallback;
    }
    return (float) node->spill_cost / (float) node->degree;
}

/* 0x004ce400; high-level equivalent; binary match unmeasured. */
InterferenceNode* Coloring_SimplifyGraph(int reg_class, int available_colors,
                                         int register_count)
{
    InterferenceNode* remaining;
    InterferenceNode* stack;
    int changed;

    stack = 0;
    do {
        changed = Coloring_SimplifyLowDegree(available_colors, register_count,
                                             &stack, &remaining);
    } while (changed);

    if (remaining != 0) {
        SpillCode_ComputeSpillCosts(reg_class);
    }

    while (remaining != 0) {
        InterferenceNode* candidate;
        InterferenceNode* node;
        float candidate_score;

        candidate = remaining;
        candidate_score = Coloring_SpillScore(candidate, gSpillScore_0056309c);
        for (node = remaining->next; node != 0; node = node->next) {
            float score;

            score = Coloring_SpillScore(node, gSpillScore_005630a0);
            if (score < candidate_score) {
                candidate = node;
                candidate_score = score;
            }
        }

        Coloring_DecrementNeighbors(candidate);
        candidate->flags |= Interference_Simplified;
        candidate->next = stack;
        stack = candidate;

        do {
            changed = Coloring_SimplifyLowDegree(
                available_colors, register_count, &stack, &remaining);
        } while (changed);
    }
    return stack;
}

static void Coloring_ResetColors(int reg_class)
{
    if (reg_class == RegClass_GPR) {
        Coloring_ResetGPRColors();
    } else if (reg_class == RegClass_FPR) {
        Coloring_ResetFPRColors();
    } else {
        Coloring_ResetVRColors();
    }
}

static unsigned int Coloring_GetColorMask(int reg_class)
{
    if (reg_class == RegClass_GPR) {
        return Coloring_GPRColorMask();
    }
    if (reg_class == RegClass_FPR) {
        return Coloring_FPRColorMask();
    }
    return Coloring_VRColorMask();
}

static short Coloring_ClaimColor(int reg_class)
{
    if (reg_class == RegClass_GPR) {
        return Coloring_ClaimGPRColor();
    }
    if (reg_class == RegClass_FPR) {
        return Coloring_ClaimFPRColor();
    }
    return Coloring_ClaimVRColor();
}

/* 0x004ce2d0; high-level equivalent; binary match unmeasured. */
int Coloring_SelectColors(int reg_class, InterferenceNode* stack)
{
    unsigned int color_mask;
    int success;

    success = 1;
    Coloring_ResetColors(reg_class);
    color_mask = Coloring_GetColorMask(reg_class);

    while (stack != 0) {
        unsigned int available;
        int index;

        available = color_mask;
        for (index = 0; index < stack->neighbor_count; index++) {
            short color;

            color =
                gInterferenceGraph[stack->neighbors[index]]->physical_register;
            if (color != -1 && color < 32) {
                available &= ~(1U << color);
            }
        }

        if (available != 0) {
            int color;

            for (color = 0; color < 32; color++) {
                if ((available & (1U << color)) != 0) {
                    stack->physical_register = (short) color;
                    break;
                }
            }
        } else {
            short color;

            color = Coloring_ClaimColor(reg_class);
            if (color == -1) {
                stack->flags |= Interference_Spilled;
                success = 0;
            } else {
                stack->physical_register = color;
                color_mask |= 1U << color;
            }
        }
        stack = stack->next;
    }
    return success;
}

static short Coloring_ResolveCoalescedColor(short color)
{
    while (color >= 32) {
        color = gInterferenceGraph[color]->physical_register;
    }
    return color;
}

/* 0x004ce1a0; high-level equivalent; binary match unmeasured. */
void Coloring_CommitAssignments(int reg_class, int register_count)
{
    PCodeBlock* block;
    int reg;

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
                    operand->reg =
                        gInterferenceGraph[operand->reg]->physical_register;
                }
            }
            if ((instruction->flags & 0x800) != 0 &&
                instruction->operands[0].reg == instruction->operands[1].reg)
            {
                PCode_RemoveRedundantInstruction(instruction);
            }
        }
    }

    for (reg = 32; reg < register_count; reg++) {
        InterferenceNode* node;

        node = gInterferenceGraph[reg];
        if (node->object != 0 && (node->flags & Interference_Spilled) == 0) {
            RegisterInfo* info;

            if ((node->flags & Interference_Coalesced) != 0) {
                node->physical_register =
                    Coloring_ResolveCoalescedColor(node->physical_register);
            }
            info = Registers_GetInfo(node->object);
            if ((node->flags & Interference_SecondOfPair) != 0) {
                info->secondary_register = node->physical_register;
            } else {
                info->physical_register = node->physical_register;
            }
        }
    }
}

static int Coloring_IsPairedGPRObject(CompilerObject* object)
{
    CompilerType* type;

    type = object->type;
    if ((type->kind == 1 || type->kind == 3) && type->size == 8) {
        return 1;
    }
    return gColoringGuard_00584244 && type->kind == 2 && type->size != 4;
}

static int Coloring_ObjectBelongsToClass(RegisterInfo* info, int reg_class)
{
    if (reg_class == RegClass_VR) {
        return info->is_vector;
    }
    if (reg_class == RegClass_FPR) {
        return info->is_fpr;
    }
    return !info->is_fpr || gColoringGuard_00584244;
}

static void Coloring_BindObjects(ObjectList* item, int reg_class)
{
    while (item != 0) {
        CompilerObject* object;
        RegisterInfo* info;
        InterferenceNode* node;

        object = item->object;
        info = Registers_GetInfo(object);
        if (info->physical_register != 0 &&
            Coloring_ObjectBelongsToClass(info, reg_class))
        {
            node = gInterferenceGraph[info->physical_register];
            node->object = object;

            if (reg_class == RegClass_GPR &&
                Coloring_IsPairedGPRObject(object))
            {
                node->flags |= Interference_FirstOfPair;
                node = gInterferenceGraph[info->secondary_register];
                node->flags |= Interference_SecondOfPair;
                node->object = object;
            }
        }
        item = item->next;
    }
}

static void Coloring_SetupClass(int reg_class)
{
    int reg;

    for (reg = 0; reg < 32; reg++) {
        gInterferenceGraph[reg]->physical_register = (short) reg;
    }
    Coloring_BindObjects(gRegisterObjectList1, reg_class);
    Coloring_BindObjects(gRegisterObjectList2, reg_class);
}

/* 0x004cdef0; functionally equivalent; binary match unmeasured. */
void Coloring_AllocateRegisters(PCodeFunction* function)
{
    Registers_SetupVRs();
    gColoringRegisterCount = gUsedVirtualRegistersVR;
    if (gUsedVirtualRegistersVR > 32 && Registers_AvailableVRs() == 0) {
        Coloring_Error(0x66, "VR");
        return;
    }
    Coloring_RunClass(function, RegClass_VR, gUsedVirtualRegistersVR,
                      Registers_AvailableVRs, Coloring_SetupVRs);

    StackFrame_CheckAltivec();
    if (gCOptimizerDumpEnabled && gHasAltivecFrame) {
        Coloring_Dump(Coloring_GetFunctionObject(function) + 10,
                      "AFTER CHECKING FOR ALTIVEC FRAME");
    }

    Registers_SetupGPRs();
    gColoringRegisterCount = gUsedVirtualRegistersGPR;
    if (gUsedVirtualRegistersGPR > 32 && Registers_AvailableGPRs() < 1) {
        Coloring_Error(0x66, "GPR");
        return;
    }
    Coloring_RunClass(function, RegClass_GPR, gUsedVirtualRegistersGPR,
                      Registers_AvailableGPRs, Coloring_SetupGPRs);

    Registers_SetupFPRs();
    gColoringRegisterCount = gUsedVirtualRegistersFPR;
    if (gColoringGuard_00584244 && gUsedVirtualRegistersFPR > 32) {
        Coloring_Assert("Coloring.c", 0x1ec);
    }
    if (gUsedVirtualRegistersFPR > 32 && Registers_AvailableFPRs() < 1) {
        Coloring_Error(0x66, "FPR");
        return;
    }
    Coloring_RunClass(function, RegClass_FPR, gUsedVirtualRegistersFPR,
                      Registers_AvailableFPRs, Coloring_SetupFPRs);

    gVirtualRegistersActive = 0;
}

/* 0x004ce5f0; high-level equivalent; target loop is unrolled. */
void Coloring_SetupVRs(void)
{
    Coloring_SetupClass(RegClass_VR);
}

/* 0x004ce710; high-level equivalent; target loop is unrolled. */
void Coloring_SetupFPRs(void)
{
    if (gColoringGuard_00584244) {
        Coloring_Assert("Coloring.c", 0x84);
    }
    Coloring_SetupClass(RegClass_FPR);
}

/* 0x004ce850; high-level equivalent; target loop is unrolled. */
void Coloring_SetupGPRs(void)
{
    Coloring_SetupClass(RegClass_GPR);
}
