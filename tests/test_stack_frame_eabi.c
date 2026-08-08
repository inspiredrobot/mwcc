#include "mwcc/StackFrameEABI.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

int gStackLocalObjectAreaSize;
static int gAlignment;

int StackFrameEABI_GetTypeAlignment(CompilerType* type)
{
    (void) type;
    return gAlignment;
}

static void TestObjectSlotOrderAndAlignment(void)
{
    CompilerType first_type;
    CompilerType second_type;
    CompilerObject first;
    CompilerObject second;

    memset(&first_type, 0, sizeof(first_type));
    memset(&second_type, 0, sizeof(second_type));
    memset(&first, 0, sizeof(first));
    memset(&second, 0, sizeof(second));
    first_type.size = 5;
    second_type.size = 4;
    first.type = &first_type;
    second.type = &second_type;

    gStackLocalObjectAreaSize = 3;
    gAlignment = 4;
    StackFrameEABI_AllocateObjectSlot(&first);
    assert(first.stack_offset == 4);
    assert(gStackLocalObjectAreaSize == 9);

    gAlignment = 8;
    StackFrameEABI_AllocateObjectSlot(&second);
    assert(second.stack_offset == 16);
    assert(gStackLocalObjectAreaSize == 20);
}

int main(void)
{
    TestObjectSlotOrderAndAlignment();
    puts("stack frame EABI tests passed");
    return 0;
}
