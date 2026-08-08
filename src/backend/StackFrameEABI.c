/*
 * StackFrameEABI.c
 *
 * Working entry points:
 *   0x004aba30  StackFrameEABI_MergePrologueEpilogue
 *   0x004abe90  StackFrameEABI_GeneratePrologueEpilogue
 *
 * Earlier helpers at 0x004a9fa0 and 0x004aaa40 participate in frame layout.
 * StackFrameEABI_AllocateObjectSlot at 0x004ac4a0 assigns addressed local
 * objects before the final frame regions are assembled at 0x004ac240.
 * Recover argument, local, temporary, spill, save-area, and outgoing-call
 * regions independently before assigning final field names.
 */

#include "mwcc/StackFrameEABI.h"

/* 0x00587c80 */
extern int gStackLocalObjectAreaSize;

/*
 * The recursive type-alignment routine at 0x004aaa40 is address-backed but
 * remains to be reconstructed as typed C.
 */

/* 0x004ac4a0 */
void StackFrameEABI_AllocateObjectSlot(CompilerObject* object)
{
    int alignment = StackFrameEABI_GetTypeAlignment(object->type);

    gStackLocalObjectAreaSize =
        (gStackLocalObjectAreaSize + alignment - 1) & ~(alignment - 1);
    object->stack_offset = gStackLocalObjectAreaSize;
    gStackLocalObjectAreaSize += object->type->size;
}
