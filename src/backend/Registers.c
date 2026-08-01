/*
 * Registers.c
 *
 * Initial target slice: 0x004c1720-0x004c2374.
 */

#include "mwcc/Registers.h"

extern void* Registers_Allocate(unsigned int size);        /* 0x00441fe0 */
extern void Registers_Zero(void* data, unsigned int size); /* 0x00441db0 */
extern void Registers_Assert(const char* file, int line);  /* 0x00445780 */

extern unsigned char gColoringGuard_00584244;
extern int gUseVirtualRegisterNumbers_00587f00;
extern short gUsedVirtualRegistersVR;
extern short gUsedVirtualRegistersGPR;
extern short gUsedVirtualRegistersFPR;

extern unsigned char gUsedPhysicalGPR[32]; /* 0x00581310 */
extern unsigned char gUsedPhysicalFPR[32]; /* 0x00581330 */
extern unsigned char gUsedPhysicalVR[32];  /* 0x00581350 */

extern short gVRSaveSpan;         /* 0x005883ea */
extern short gFPRSaveSpan;        /* 0x00588438 */
extern short gGPRSaveSpan;        /* 0x0058843a */
extern short gAvailableSavedFPRs; /* 0x00588466 */
extern short gAvailableSavedGPRs; /* 0x00588468 */
extern short gAvailableSavedVRs;  /* 0x0058849c */

static void Registers_Require(int condition, int line)
{
    if (!condition) {
        Registers_Assert("Registers.c", line);
    }
}

/* 0x004c1720; functionally equivalent; binary match unmeasured. */
RegisterInfo* Registers_GetInfo(CompilerObject* object)
{
    RegisterInfo* info;

    switch (object->kind) {
    case 0:
    case 2:
        if (object->register_info_2e == 0) {
            info = Registers_Allocate(sizeof(RegisterInfo));
            Registers_Zero(info, sizeof(RegisterInfo));
            object->register_info_2e = info;
        }
        return object->register_info_2e;

    case 1:
        Registers_Require(object->register_info_26 != 0, 0x2e9);
        return object->register_info_26;

    default:
        Registers_Assert("Registers.c", 0x2f6);
        return 0;
    }
}

static void Registers_RecordPhysicalUse(unsigned char* used, short reg,
                                        int scan_floor, int saved_floor,
                                        short* save_span,
                                        short* available_saved)
{
    int first_used;
    int free_count;
    int scan;

    if (used[reg] != 0) {
        return;
    }

    used[reg] = 1;
    first_used = 32;
    free_count = 0;
    for (scan = 31; scan >= scan_floor; scan--) {
        if (used[scan] == 1) {
            first_used = scan;
        }
        if (scan > saved_floor && used[scan] == 0) {
            free_count++;
        }
    }
    *save_span = (short) (32 - first_used);
    *available_saved = (short) free_count;
}

static int Registers_ObjectUsesFPRs(CompilerObject* object)
{
    return gColoringGuard_00584244 && object->type->kind == 2;
}

/* 0x004c1b40; high-level equivalent; binary match unmeasured. */
void Registers_BindVR(CompilerObject* object, short reg)
{
    RegisterInfo* info;

    Registers_RecordPhysicalUse(gUsedPhysicalVR, reg, 20, 19, &gVRSaveSpan,
                                &gAvailableSavedVRs);
    if (object == 0) {
        return;
    }

    info = Registers_GetInfo(object);
    info->is_fpr = 0;
    info->is_vector = 1;
    info->physical_register = reg;
}

/* 0x004c1c50; high-level equivalent; binary match unmeasured. */
void Registers_BindFPR(CompilerObject* object, short reg)
{
    RegisterInfo* info;

    Registers_RecordPhysicalUse(gUsedPhysicalFPR, reg, 14, 17, &gFPRSaveSpan,
                                &gAvailableSavedFPRs);
    if (object == 0) {
        return;
    }

    info = Registers_GetInfo(object);
    info->is_fpr = 1;
    info->is_vector = 0;
    info->physical_register = reg;
}

/* 0x004c1d60; high-level equivalent; binary match unmeasured. */
void Registers_BindGPRPair(CompilerObject* object, short first, short second)
{
    RegisterInfo* info;

    Registers_BindGPR(0, first);
    Registers_BindGPR(0, second);
    if (object == 0) {
        return;
    }

    info = Registers_GetInfo(object);
    info->is_fpr = 0;
    info->is_vector = 0;
    info->physical_register = first;
    info->secondary_register = second;
    if (Registers_ObjectUsesFPRs(object)) {
        info->is_fpr = 1;
    }
}

/* 0x004c1e40; high-level equivalent; binary match unmeasured. */
void Registers_BindGPR(CompilerObject* object, short reg)
{
    RegisterInfo* info;

    Registers_RecordPhysicalUse(gUsedPhysicalGPR, reg, 14, 15, &gGPRSaveSpan,
                                &gAvailableSavedGPRs);
    if (object == 0) {
        return;
    }

    info = Registers_GetInfo(object);
    info->is_fpr = 0;
    info->is_vector = 0;
    info->physical_register = reg;
    if (Registers_ObjectUsesFPRs(object)) {
        info->is_fpr = 1;
    }
}

static short Registers_FindFree(unsigned char* used, int first,
                                void (*bind)(CompilerObject*, short))
{
    int reg;

    for (reg = 31; reg >= first; reg--) {
        if (used[reg] == 0) {
            bind(0, (short) reg);
            return (short) reg;
        }
    }
    return -1;
}

/* 0x004c1f60; high-level equivalent; binary match unmeasured. */
void Registers_AllocateVR(CompilerObject* object)
{
    RegisterInfo* info;
    short reg;

    info = Registers_GetInfo(object);
    if (gUseVirtualRegisterNumbers_00587f00) {
        reg = gUsedVirtualRegistersVR++;
    } else {
        reg = Registers_FindFree(gUsedPhysicalVR, 20, Registers_BindVR);
    }

    info->is_fpr = 0;
    info->is_vector = 1;
    if (reg > 0) {
        info->physical_register = reg;
    }
}

/* 0x004c2040; high-level equivalent; binary match unmeasured. */
void Registers_AllocateFPR(CompilerObject* object)
{
    RegisterInfo* info;
    short reg;

    info = Registers_GetInfo(object);
    if (gUseVirtualRegisterNumbers_00587f00) {
        reg = gUsedVirtualRegistersFPR++;
    } else {
        reg = Registers_FindFree(gUsedPhysicalFPR, 14, Registers_BindFPR);
    }

    info->is_fpr = 1;
    info->is_vector = 0;
    if (reg > 0) {
        info->physical_register = reg;
    }
}

/* 0x004c2120; high-level equivalent; binary match unmeasured. */
void Registers_AllocateGPRPair(CompilerObject* object)
{
    RegisterInfo* info;
    short first;
    short second;

    info = Registers_GetInfo(object);
    if (gUseVirtualRegisterNumbers_00587f00) {
        first = gUsedVirtualRegistersGPR++;
        second = gUsedVirtualRegistersGPR++;
    } else {
        Registers_Require(gAvailableSavedGPRs >= 2, 0xb9);
        first = Registers_FindFree(gUsedPhysicalGPR, 14, Registers_BindGPR);
        second = Registers_FindFree(gUsedPhysicalGPR, 14, Registers_BindGPR);
    }

    info->is_fpr = 0;
    info->is_vector = 0;
    if (Registers_ObjectUsesFPRs(object)) {
        info->is_fpr = 1;
    }
    if (first > 0 && second > 0) {
        info->physical_register = first;
        info->secondary_register = second;
    }
}

/* 0x004c2280; high-level equivalent; binary match unmeasured. */
void Registers_AllocateGPR(CompilerObject* object)
{
    RegisterInfo* info;
    short reg;

    info = Registers_GetInfo(object);
    if (gUseVirtualRegisterNumbers_00587f00) {
        reg = gUsedVirtualRegistersGPR++;
    } else {
        reg = Registers_FindFree(gUsedPhysicalGPR, 14, Registers_BindGPR);
    }

    info->is_fpr = 0;
    info->is_vector = 0;
    if (Registers_ObjectUsesFPRs(object)) {
        info->is_fpr = 1;
    }
    if (reg > 0) {
        info->physical_register = reg;
    }
}
