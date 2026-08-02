#ifndef MWCC_OPERANDS_H
#define MWCC_OPERANDS_H

#include "mwcc/backend_types.h"

void Operands_ForceGPR(Operand* operand, CompilerType* type,
                       short requested_register);
void Operands_ForceFPR(Operand* operand, CompilerType* type,
                       short requested_register);

#endif
