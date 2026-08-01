/*
 * Coloring.c
 *
 * Working entry points:
 *   0x004cdef0  Coloring_AllocateRegisters
 *   0x004ce710  Coloring_004ce710
 *
 * The coordinator colors vector, floating-point, and general-purpose classes
 * separately and retries after inserting spill code. Preserve that class
 * split while recovering interference, coalescing, coloring, and spill choice.
 */
