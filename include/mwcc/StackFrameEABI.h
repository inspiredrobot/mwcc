#ifndef MWCC_STACK_FRAME_EABI_H
#define MWCC_STACK_FRAME_EABI_H

#include "mwcc/backend_types.h"

int StackFrameEABI_GetTypeAlignment(CompilerType* type);
void StackFrameEABI_AllocateObjectSlot(CompilerObject* object);

#endif
