/*
 * Frontend expression/object bridge recovered from COptimizer.c.
 *
 * A CodeGen item carries an expression pointer at +0x0a. This walk follows
 * that expression graph to source CompilerObject identities and updates the
 * same RegisterInfo fields later consumed by code motion and allocation.
 */

#include "mwcc/COptimizerExpressions.h"

extern void COptimizer_Assert(const char* file, int line); /* 0x00445780 */

extern unsigned char gCOptimizerCountUsesAsOne_005842e2;
extern unsigned char gCOptimizerExpressionScanFlag_00588513;
extern unsigned int gCOptimizerCurrentWeight_00587160;

/* 0x004beef0; reconstructed from the stock GC/1.2.5 executable. */
void COptimizer_RecordObjectUse(CompilerObject* object,
                                unsigned char direct_reference)
{
    RegisterInfo* info;

    if (object->kind == 6) {
        COptimizer_Assert("COptimizer.c", 0x73a);
    }
    if (object->kind != 1) {
        return;
    }

    info = object->register_info_26;
    info->flags_23 = 1;
    if (gCOptimizerCountUsesAsOne_005842e2) {
        info->weight_04++;
    } else {
        info->weight_04 += gCOptimizerCurrentWeight_00587160;
    }
    if (direct_reference) {
        info->flags_22 = 1;
    }
}

/* 0x004beda0; reconstructed from the stock GC/1.2.5 executable. */
void COptimizer_CountExpressionObjectUses(CExpression* expression)
{
    CExpressionList* list;

    for (;;) {
        switch (expression->kind) {
        case 0:
        case 1:
        case 2:
        case 3:
        case 5:
        case 6:
        case 7:
        case 8:
        case 0x30:
        case 0x31:
            expression = expression->value_0a.expression;
            break;

        case 4:
            expression = expression->value_0a.expression;
            if (expression->kind != CExpressionKind_ObjectReference) {
                break;
            }
            COptimizer_RecordObjectUse(expression->value_0a.object, 0);
            return;

        case 9:
        case 0x0a:
        case 0x0b:
        case 0x0c:
        case 0x0d:
        case 0x0e:
        case 0x0f:
        case 0x10:
        case 0x11:
        case 0x12:
        case 0x13:
        case 0x14:
        case 0x15:
        case 0x16:
        case 0x17:
        case 0x18:
        case 0x19:
        case 0x1a:
        case 0x1b:
        case 0x1c:
        case 0x1d:
        case 0x1e:
        case 0x1f:
        case 0x20:
        case 0x21:
        case 0x22:
        case 0x23:
        case 0x24:
        case 0x25:
        case 0x26:
        case 0x27:
        case 0x28:
        case 0x29:
        case 0x2a:
        case 0x2b:
        case 0x2c:
        case 0x2d:
        case 0x2e:
        case 0x2f:
            COptimizer_CountExpressionObjectUses(
                expression->value_0a.expression);
            expression = expression->value_0e.expression;
            break;

        case 0x32:
        case 0x33:
        case 0x3b:
        case 0x3f:
        case 0x4a:
        case 0x34:
            return;

        case 0x35:
            COptimizer_CountExpressionObjectUses(
                expression->value_0a.expression);
            COptimizer_CountExpressionObjectUses(
                expression->value_0e.expression);
            COptimizer_CountExpressionObjectUses(
                expression->value_12.expression);
            return;

        case 0x36:
        case 0x37:
            gCOptimizerExpressionScanFlag_00588513 = 0;
            COptimizer_CountExpressionObjectUses(
                expression->value_0a.expression);
            for (list = expression->value_0e.list; list != 0;
                 list = list->next)
            {
                COptimizer_CountExpressionObjectUses(list->expression);
            }
            return;

        case CExpressionKind_ObjectReference:
            COptimizer_RecordObjectUse(expression->value_0a.object, 1);
            return;

        case 0x3a:
            COptimizer_CountExpressionObjectUses(
                expression->value_0a.expression);
            expression = expression->value_0e.expression;
            break;

        default:
            COptimizer_Assert("COptimizer.c", 0x79d);
            break;
        }
    }
}
