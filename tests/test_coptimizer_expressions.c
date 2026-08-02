#include "mwcc/COptimizerExpressions.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

unsigned char gCOptimizerCountUsesAsOne_005842e2;
unsigned char gCOptimizerExpressionScanFlag_00588513;
unsigned int gCOptimizerCurrentWeight_00587160;

static int gAssertCount;
static int gAssertLine;

static void Check(int condition, const char* message)
{
    if (!condition) {
        fprintf(stderr, "COptimizer expression test failed: %s\n", message);
        exit(1);
    }
}

void COptimizer_Assert(const char* file, int line)
{
    Check(strcmp(file, "COptimizer.c") == 0, "assert source");
    gAssertCount++;
    gAssertLine = line;
}

static void Reset(void)
{
    gCOptimizerCountUsesAsOne_005842e2 = 0;
    gCOptimizerExpressionScanFlag_00588513 = 1;
    gCOptimizerCurrentWeight_00587160 = 7;
    gAssertCount = 0;
    gAssertLine = 0;
}

static void SetObjectReference(CExpression* expression, CompilerObject* object)
{
    memset(expression, 0, sizeof(*expression));
    expression->kind = CExpressionKind_ObjectReference;
    expression->value_0a.object = object;
}

static void TestObjectUse(void)
{
    RegisterInfo info;
    CompilerObject object;

    Reset();
    memset(&info, 0, sizeof(info));
    memset(&object, 0, sizeof(object));
    object.kind = 1;
    object.register_info_26 = &info;

    COptimizer_RecordObjectUse(&object, 1);
    Check(info.weight_04 == 7, "weighted use");
    Check(info.flags_22 == 1 && info.flags_23 == 1, "direct flags");

    gCOptimizerCountUsesAsOne_005842e2 = 1;
    COptimizer_RecordObjectUse(&object, 0);
    Check(info.weight_04 == 8, "unit use");

    object.kind = 6;
    COptimizer_RecordObjectUse(&object, 0);
    Check(gAssertCount == 1 && gAssertLine == 0x73a,
          "invalid object assertion");
}

static void TestExpressionWalk(void)
{
    RegisterInfo first_info;
    RegisterInfo second_info;
    CompilerObject first_object;
    CompilerObject second_object;
    CExpression root;
    CExpression binary;
    CExpression first_reference;
    CExpression second_reference;
    CExpressionList list;

    Reset();
    memset(&first_info, 0, sizeof(first_info));
    memset(&second_info, 0, sizeof(second_info));
    memset(&first_object, 0, sizeof(first_object));
    memset(&second_object, 0, sizeof(second_object));
    first_object.kind = 1;
    first_object.register_info_26 = &first_info;
    second_object.kind = 1;
    second_object.register_info_26 = &second_info;
    SetObjectReference(&first_reference, &first_object);
    SetObjectReference(&second_reference, &second_object);

    memset(&binary, 0, sizeof(binary));
    binary.kind = 9;
    binary.value_0a.expression = &first_reference;
    binary.value_0e.expression = &second_reference;
    COptimizer_CountExpressionObjectUses(&binary);
    Check(first_info.weight_04 == 7 && second_info.weight_04 == 7,
          "binary child walk");

    memset(&list, 0, sizeof(list));
    list.expression = &second_reference;
    memset(&root, 0, sizeof(root));
    root.kind = 0x36;
    root.value_0a.expression = &first_reference;
    root.value_0e.list = &list;
    COptimizer_CountExpressionObjectUses(&root);
    Check(first_info.weight_04 == 14 && second_info.weight_04 == 14,
          "expression list walk");
    Check(gCOptimizerExpressionScanFlag_00588513 == 0,
          "list expression scan flag");
}

static void TestWrappedReference(void)
{
    RegisterInfo info;
    CompilerObject object;
    CExpression wrapper;
    CExpression reference;

    Reset();
    memset(&info, 0, sizeof(info));
    memset(&object, 0, sizeof(object));
    object.kind = 1;
    object.register_info_26 = &info;
    SetObjectReference(&reference, &object);
    memset(&wrapper, 0, sizeof(wrapper));
    wrapper.kind = 4;
    wrapper.value_0a.expression = &reference;

    COptimizer_CountExpressionObjectUses(&wrapper);
    Check(info.weight_04 == 7, "wrapped reference use");
    Check(info.flags_22 == 0 && info.flags_23 == 1, "wrapped reference flags");
}

int main(void)
{
    TestObjectUse();
    TestExpressionWalk();
    TestWrappedReference();
    return 0;
}
