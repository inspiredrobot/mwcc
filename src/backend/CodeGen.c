/*
 * CodeGen.c
 *
 * Core entry point:
 *   0x004351c0  CodeGen_Generator
 *
 * This function coordinates PCode construction, backend optimization,
 * scheduling, register coloring, EABI frame construction, final peephole
 * optimization, and emission. Recover the coordinator before its large
 * helpers so every pass boundary remains visible and testable.
 */

typedef struct CodeGen_DecompPending CodeGen_DecompPending;
