#ifndef MWCC_COLORING_H
#define MWCC_COLORING_H

#include "mwcc/backend_types.h"

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

void Coloring_AllocateRegisters(PCodeFunction* function);
void Coloring_CommitAssignments(int reg_class, int register_count);
int Coloring_SelectColors(int reg_class, InterferenceNode* stack);
InterferenceNode* Coloring_SimplifyGraph(int reg_class, int available_colors,
                                         int register_count);
void Coloring_SetupVRs(void);
void Coloring_SetupFPRs(void);
void Coloring_SetupGPRs(void);

#endif
