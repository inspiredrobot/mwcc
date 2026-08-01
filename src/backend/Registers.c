/*
 * Registers.c
 *
 * Initial target slice: 0x004c1720-0x004c2374.
 */

#include "mwcc/backend_types.h"

extern void* Registers_Allocate(unsigned int size);        /* 0x00441fe0 */
extern void Registers_Zero(void* data, unsigned int size); /* 0x00441db0 */
extern void Registers_Assert(const char* file, int line);  /* 0x00445780 */

#define REGISTERS_ASSERT(condition, line)                                     \
    do {                                                                      \
        if (!(condition)) {                                                   \
            Registers_Assert("Registers.c", (line));                          \
        }                                                                     \
    } while (0)

/* 0x004c1720; functionally equivalent; binary match unmeasured. */
RegisterInfo* Registers_GetInfo(CompilerObject* object)
{
    RegisterInfo* info;

    switch (object->kind) {
    case 0:
    case 2:
        if (object->register_info_2e == 0) {
            info = Registers_Allocate(sizeof(RegisterInfo));
            Registers_Zero(info, sizeof(RegisterInfo));
            object->register_info_2e = info;
        }
        return object->register_info_2e;

    case 1:
        REGISTERS_ASSERT(object->register_info_26 != 0, 0x2e9);
        return object->register_info_26;

    default:
        Registers_Assert("Registers.c", 0x2f6);
        return 0;
    }
}
