#ifndef MWCC_REGISTERS_H
#define MWCC_REGISTERS_H

#include "mwcc/backend_types.h"

RegisterInfo* Registers_GetInfo(CompilerObject* object);

void Registers_BindVR(CompilerObject* object, short reg);
void Registers_BindFPR(CompilerObject* object, short reg);
void Registers_BindGPR(CompilerObject* object, short reg);
void Registers_BindGPRPair(CompilerObject* object, short first, short second);

void Registers_AllocateVR(CompilerObject* object);
void Registers_AllocateFPR(CompilerObject* object);
void Registers_AllocateGPR(CompilerObject* object);
void Registers_AllocateGPRPair(CompilerObject* object);

#endif
