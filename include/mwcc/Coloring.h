#ifndef MWCC_COLORING_H
#define MWCC_COLORING_H

#include "mwcc/backend_types.h"

void Coloring_AllocateRegisters(PCodeFunction* function);
void Coloring_SetupVRs(void);
void Coloring_SetupFPRs(void);
void Coloring_SetupGPRs(void);

#endif
