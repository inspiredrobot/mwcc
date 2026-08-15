/*
 * Scheduler.c
 *
 * Working entry points (full algorithm write-up in docs/SCHEDULER.md; a
 * validated Python model lives in (case study withheld)):
 *   0x004ccae0  Scheduler_Schedule (driver, machine-model select)
 *   0x004ccbf0  Scheduler_ScheduleBlock (cycle-driven list scheduler)
 *   0x004ccdc0  Scheduler_PickInstruction (4-level strict-win tie-break)
 *   0x004ccf10  Scheduler_BuildDependencies (backward walk, exact heights)
 *   0x004cd2f0  Scheduler_AddWildcardMemoryDeps
 *   0x004cd4a0  Scheduler_AddObjectMemoryDeps
 *   0x004cd650  Scheduler_AddVolatileMemoryDeps
 *   0x004cd7c0  Scheduler_AddRegisterDeps
 *   0x004cd910  Scheduler_AddEdge
 *   0x004cd9d0  Scheduler_ResetBlockState
 *   0x004cca50  Scheduler_DependenceTestStub (always returns 0, dead)
 *
 * CodeGen_Generator calls the scheduler between exact before/after trace
 * markers. -proc gekko sets CPU byte 8, which has no case in the driver's
 * model switch, so GameCube compiles schedule with the DEFAULT machine model
 * at 0x574d70: issue width 2, one unit per class (single integer unit),
 * two-stage LSU, three-stage FPU, a five-entry in-order completion ring with
 * at most two retires per cycle, WAR/WAW latency zero, branches as barriers.
 * Per-opcode records are 6 bytes at 0x574d90 + opcode * 6:
 * [unit, latency, occupancy, stage2, stage3, serialize].
 */

typedef struct Scheduler_DecompPending Scheduler_DecompPending;
