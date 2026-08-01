/*
 * StackFrameEABI.c
 *
 * Working entry points:
 *   0x004aba30  StackFrameEABI_MergePrologueEpilogue
 *   0x004abe90  StackFrameEABI_GeneratePrologueEpilogue
 *
 * Earlier helpers at 0x004a9fa0 and 0x004aaa40 participate in frame layout.
 * Recover argument, local, temporary, spill, save-area, and outgoing-call
 * regions independently before assigning final field names.
 */
