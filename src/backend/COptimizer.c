/*
 * COptimizer.c
 *
 * Working entry points:
 *   0x004c4430  COptimizer_Optimize
 *   0x004c4530  COptimizer_Level4
 *   0x004c4910  COptimizer_Level3
 *
 * The names of individual passes are anchored by the adjacent trace strings.
 * Repeated passes are intentional and must not be folded together.
 */
