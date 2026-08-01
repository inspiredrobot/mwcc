#ifndef MWCC_SPILL_CODE_H
#define MWCC_SPILL_CODE_H

#include "mwcc/backend_types.h"

void SpillCode_BuildInterference(PCodeFunction* function, int reg_class,
                                 int register_count);
void SpillCode_MarkLastUses(int reg_class, int register_count);
void SpillCode_ConstructInterference(int reg_class, int register_count);
void SpillCode_CoalesceCopies(int reg_class, int register_count);
void SpillCode_MaterializeGraph(int register_count);
void SpillCode_ComputeSpillCosts(int reg_class);

#endif
