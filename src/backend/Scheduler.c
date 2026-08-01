/*
 * Scheduler.c
 *
 * Working entry points:
 *   0x004ccae0  Scheduler_Schedule
 *   0x004ccf10  Scheduler_004ccf10
 *
 * CodeGen_Generator calls the scheduler between exact before/after trace
 * markers. The helper at 0x004ccf10 walks packed 12-byte PCode operands and
 * constructs register and memory dependencies.
 */

typedef struct Scheduler_DecompPending Scheduler_DecompPending;
