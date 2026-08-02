#ifndef MWCC_PCODE_H
#define MWCC_PCODE_H

#include "mwcc/backend_types.h"

typedef union PCodeBuildArgument {
    int signed_value;
    unsigned int unsigned_value;
    CompilerObject* object;
} PCodeBuildArgument;

PCodeInstruction* PCode_CloneInstruction(PCodeInstruction* source);
PCodeInstruction*
PCodeUtilities_BuildInstructionV(short opcode, PCodeBuildArgument* arguments);

#endif
