/*
 * SpillCode.c
 *
 * Direct source-file anchors identify six initial functions at
 * 0x00531ab0-0x00532774. Recover these with the Coloring.c retry paths so the
 * distinction between spill selection and spill-code insertion stays clear.
 */

typedef struct SpillCode_DecompPending SpillCode_DecompPending;
