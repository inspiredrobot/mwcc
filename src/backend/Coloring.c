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

enum RegisterClass {
    RegClass_GPR = 0,
    RegClass_FPR = 1,
    RegClass_VR = 9
};

enum InterferenceFlags {
    Interference_Spilled = 0x01,
    Interference_Simplified = 0x02,
    Interference_Coalesced = 0x04,
    Interference_SecondOfPair = 0x10,
    Interference_FirstOfPair = 0x20
};

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

extern void SpillCode_BuildInterference(PCodeFunction* function, int reg_class,
                                        int register_count); /* 0x00530a00 */
extern void Coloring_SetupVRs(void);                         /* 0x004ce5f0 */
extern void Coloring_SetupGPRs(void);                        /* 0x004ce850 */
extern void Coloring_SetupFPRs(void);                        /* 0x004ce710 */
extern int* Coloring_004ce400(int reg_class, int available,
                              int register_count);
extern int Coloring_004ce2d0(int reg_class, int* graph);
extern void Coloring_004ce1a0(int reg_class, int register_count);
extern void SpillCode_00531800(int reg_class, int register_count);
extern void Coloring_FreeIteration(void);  /* 0x00441e20 */
extern void StackFrame_CheckAltivec(void); /* 0x004a9c80 */

extern InterferenceNode** gInterferenceGraph; /* 0x00587e3c */
extern ObjectList* gRegisterObjectList1;      /* 0x0058806c */
extern ObjectList* gRegisterObjectList2;      /* 0x00587fb8 */
static void Coloring_RunClass(PCodeFunction* function, int reg_class,
                              int register_count,
                              int (*available_registers)(void),
                              void (*setup_class)(void))
{
    int retry;
    int* graph;

    retry = 1;
    while (retry && register_count > 32) {
        SpillCode_BuildInterference(function, reg_class, register_count);
        setup_class();
        retry = 0;
        graph = Coloring_004ce400(reg_class, available_registers(),
                                  register_count);
        if (!Coloring_004ce2d0(reg_class, graph)) {
            retry = 1;
        }
        if (retry) {
            SpillCode_00531800(reg_class, register_count);
        } else {
            Coloring_004ce1a0(reg_class, register_count);
        }
        Coloring_FreeIteration();
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
