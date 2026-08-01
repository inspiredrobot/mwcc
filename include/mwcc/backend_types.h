#ifndef MWCC_BACKEND_TYPES_H
#define MWCC_BACKEND_TYPES_H

#include <stddef.h>

typedef struct PCodeFunction PCodeFunction;

#pragma pack(push, 2)

typedef struct RegisterInfo {
    unsigned char unknown_00[0x24];
    short physical_register; /* 0x24 */
    unsigned char unknown_26[2];
    unsigned char is_fpr; /* 0x28 */
    unsigned char unknown_29;
    unsigned char flag_2a; /* 0x2a */
    unsigned char unknown_2b;
} RegisterInfo;

typedef struct CompilerObject {
    unsigned char unknown_00[2];
    unsigned char kind; /* 0x02 */
    unsigned char unknown_03[0x23];
    RegisterInfo* register_info_26; /* 0x26 */
    unsigned char unknown_2a[4];
    RegisterInfo* register_info_2e; /* 0x2e */
} CompilerObject;

typedef struct ObjectList {
    struct ObjectList* next; /* 0x00 */
    CompilerObject* object;  /* 0x04 */
} ObjectList;

typedef struct InterferenceNode {
    unsigned char unknown_00[4];
    CompilerObject* object; /* 0x04 */
    unsigned char unknown_08[8];
    short physical_register; /* 0x10 */
} InterferenceNode;

#pragma pack(pop)

typedef char RegisterInfo_size_2c[(sizeof(RegisterInfo) == 0x2c) ? 1 : -1];
typedef char RegisterInfo_physical_24
    [(offsetof(RegisterInfo, physical_register) == 0x24) ? 1 : -1];
typedef char CompilerObject_info_26
    [(offsetof(CompilerObject, register_info_26) == 0x26) ? 1 : -1];
typedef char CompilerObject_info_2e
    [(offsetof(CompilerObject, register_info_2e) == 0x2e) ? 1 : -1];
typedef char InterferenceNode_physical_10
    [(offsetof(InterferenceNode, physical_register) == 0x10) ? 1 : -1];

#endif
