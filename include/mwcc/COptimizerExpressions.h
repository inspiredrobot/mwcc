#ifndef MWCC_COPTIMIZER_EXPRESSIONS_H
#define MWCC_COPTIMIZER_EXPRESSIONS_H

#include "mwcc/frontend_types.h"

void COptimizer_RecordObjectUse(CompilerObject* object,
                                unsigned char direct_reference);
void COptimizer_CountExpressionObjectUses(CExpression* expression);

#endif
