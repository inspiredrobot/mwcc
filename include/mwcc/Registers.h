#ifndef MWCC_REGISTERS_H
#define MWCC_REGISTERS_H

#include "mwcc/backend_types.h"

RegisterInfo* Registers_GetInfo(CompilerObject* object);

void Coloring_ResetVRColors(void);
void Coloring_ResetFPRColors(void);
void Coloring_ResetGPRColors(void);

void Registers_SetupVRs(void);
void Registers_SetupFPRs(void);
void Registers_SetupGPRs(void);

short Coloring_ClaimVRColor(void);
short Coloring_ClaimFPRColor(void);
short Coloring_ClaimGPRColor(void);

unsigned int Coloring_VRColorMask(void);
unsigned int Coloring_FPRColorMask(void);
unsigned int Coloring_GPRColorMask(void);

int Registers_AvailableVRs(void);
int Registers_AvailableFPRs(void);
int Registers_AvailableGPRs(void);

void Registers_BindVR(CompilerObject* object, short reg);
void Registers_BindFPR(CompilerObject* object, short reg);
void Registers_BindGPR(CompilerObject* object, short reg);
void Registers_BindGPRPair(CompilerObject* object, short first, short second);

void Registers_AllocateVR(CompilerObject* object);
void Registers_AllocateFPR(CompilerObject* object);
void Registers_AllocateGPR(CompilerObject* object);
void Registers_AllocateGPRPair(CompilerObject* object);

#endif
