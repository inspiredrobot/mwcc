/*
 * COptimizer.c
 *
 * Working entry points:
 *   0x004c4430  COptimizer_Optimize
 *   0x004c4530  COptimizer_Level4
 *   0x004c4910  COptimizer_Level3
 *
 * Pass roles are confirmed by adjacent trace strings. Address-suffixed helper
 * names remain in places where the exact operation is not yet established.
 */

#include "mwcc/backend_types.h"

extern unsigned char gCOptimizerDumpEnabled; /* 0x00584226 */
extern unsigned char gOptimizationLevel;     /* 0x005842e1 */
extern unsigned char gVectorArrayConversion; /* 0x00588522 */
extern int gRunLevel2Pipeline;               /* 0x00588280 */
extern int gValueNumberingChanged;           /* 0x005875c0 */
extern int gCopyPropagationChanged;          /* 0x005875d0 */
extern int gAddPropagationChanged;           /* 0x00587f8c */
extern int gCodeMotionChanged;               /* 0x005875b0 */
extern int gLoopCodeMotionEnabled;           /* 0x0058763c */
extern int gStrengthReductionChanged;        /* 0x00587ec4 */
extern int gLoopTransformChanged;            /* 0x0058807c */
extern int gConstantPropagationChanged;      /* 0x0058826c */
extern int gLoadDeletionChanged;             /* 0x00587194 */
extern int gArrayToRegisterEnabled;          /* 0x00587110 */
extern int gArrayToRegisterChanged;          /* 0x0058801c */

extern char* COptimizer_GetFunctionObject(PCodeFunction* function);
extern void COptimizer_Dump(const char* function_name, const char* stage);

extern void COpt_ValueNumbering(int mode);  /* 0x0051e590 */
extern void COpt_CopyPropagation(int mode); /* 0x005200e0 */
extern void COpt_AddPropagation(void);      /* 0x00520bf0 */
extern void COpt_00521a10(void);
extern void COpt_00521d10(int value);
extern void COpt_00522990(void);
extern void COpt_SetLoopCodeMotionMode(int mode); /* 0x00523650 */
extern void COpt_00524bd0(void);
extern void COpt_StrengthReduction(void);    /* 0x005270c0 */
extern void COpt_LoopTransformations(void);  /* 0x005289b0 */
extern void COpt_ArrayToRegister(void);      /* 0x00528bb0 */
extern void COpt_ConstantPropagation(void);  /* 0x0052b530 */
extern void COpt_LoadDeletion(void);         /* 0x0052c780 */
extern int COpt_VectorArrayConversion(void); /* 0x0052ce10 */

void COptimizer_Level3(PCodeFunction* function);
void COptimizer_Level4(PCodeFunction* function);

static void COptimizer_DumpStage(PCodeFunction* function, const char* stage)
{
    COptimizer_Dump(COptimizer_GetFunctionObject(function) + 10, stage);
}

static void COptimizer_DumpIfChanged(PCodeFunction* function, int changed,
                                     const char* stage)
{
    if (changed && gCOptimizerDumpEnabled) {
        COptimizer_DumpStage(function, stage);
    }
}

static void COptimizer_RunCopyPropagation(PCodeFunction* function, int mode)
{
    COpt_CopyPropagation(mode);
    COptimizer_DumpIfChanged(function, gCopyPropagationChanged,
                             "AFTER COPY PROPAGATION");
}

static void COptimizer_RunAddPropagation(PCodeFunction* function)
{
    COpt_AddPropagation();
    COptimizer_DumpIfChanged(function, gAddPropagationChanged,
                             "AFTER ADD PROPAGATION");
}

static void COptimizer_RunLoopPasses(PCodeFunction* function)
{
    COpt_00522990();
    if (gLoopCodeMotionEnabled) {
        COpt_SetLoopCodeMotionMode(1);
        COpt_00521a10();
        COpt_00524bd0();
        COpt_SetLoopCodeMotionMode(0);
        COpt_00521d10(gLoopCodeMotionEnabled);
        COptimizer_DumpIfChanged(function, gCodeMotionChanged,
                                 "AFTER CODE MOTION");

        COpt_StrengthReduction();
        if (gStrengthReductionChanged) {
            COpt_CopyPropagation(1);
            if (gCOptimizerDumpEnabled) {
                COptimizer_DumpStage(function, "AFTER STRENGTH REDUCTION");
            }
        }

        COpt_LoopTransformations();
        if (gLoopTransformChanged) {
            COpt_CopyPropagation(1);
            COpt_AddPropagation();
            if (gCOptimizerDumpEnabled) {
                COptimizer_DumpStage(function, "AFTER LOOP TRANSFORMATIONS");
            }
        }
    }

    if (!gCopyPropagationChanged) {
        COptimizer_RunCopyPropagation(function, 1);
    }
}

/* 0x004c4430; functionally equivalent; binary match unmeasured. */
void COptimizer_Optimize(PCodeFunction* function)
{
    if (gCOptimizerDumpEnabled) {
        COptimizer_DumpStage(function, "BEFORE GLOBAL OPTIMIZATION");
    }

    if (gOptimizationLevel == 2 ||
        (gRunLevel2Pipeline && gOptimizationLevel > 2))
    {
        COpt_ValueNumbering(1);
        COptimizer_DumpIfChanged(function, gValueNumberingChanged,
                                 "AFTER CSE");
        COptimizer_RunCopyPropagation(function, 1);
        COptimizer_RunAddPropagation(function);
    } else if (gOptimizationLevel == 3) {
        COptimizer_Level3(function);
    } else if (gOptimizationLevel == 4) {
        COptimizer_Level4(function);
    }
}

/* 0x004c4910; functionally equivalent; binary match unmeasured. */
void COptimizer_Level3(PCodeFunction* function)
{
    COpt_ValueNumbering(0);
    COptimizer_DumpIfChanged(function, gValueNumberingChanged,
                             "AFTER VALUE NUMBERING");
    COptimizer_RunCopyPropagation(function, 0);

    gCopyPropagationChanged = 0;
    COptimizer_RunAddPropagation(function);
    COptimizer_RunLoopPasses(function);

    COpt_ConstantPropagation();
    if (gConstantPropagationChanged) {
        if (gCOptimizerDumpEnabled) {
            COptimizer_DumpStage(function, "AFTER CONSTANT PROPAGATION");
        }
        COpt_LoadDeletion();
        COptimizer_DumpIfChanged(function, gLoadDeletionChanged,
                                 "AFTER LOAD DELETION");
        COptimizer_RunAddPropagation(function);
    }

    COpt_ValueNumbering(1);
    if (gValueNumberingChanged) {
        COpt_CopyPropagation(1);
        if (gCOptimizerDumpEnabled) {
            COptimizer_DumpStage(function, "AFTER VALUE NUMBERING 2");
        }
    }
}

/* 0x004c4530; functionally equivalent; binary match unmeasured. */
void COptimizer_Level4(PCodeFunction* function)
{
    COpt_ValueNumbering(0);
    COptimizer_DumpIfChanged(function, gValueNumberingChanged,
                             "AFTER VALUE NUMBERING");
    COptimizer_RunCopyPropagation(function, 0);

    gCopyPropagationChanged = 0;
    COptimizer_RunAddPropagation(function);
    COptimizer_RunLoopPasses(function);

    COpt_ConstantPropagation();
    if (gConstantPropagationChanged) {
        if (gCOptimizerDumpEnabled) {
            COptimizer_DumpStage(function, "AFTER CONSTANT PROPAGATION");
        }
        COpt_LoadDeletion();
        COptimizer_DumpIfChanged(function, gLoadDeletionChanged,
                                 "AFTER LOAD DELETATION");

        if (gConstantPropagationChanged) {
            COpt_CopyPropagation(1);
        }
        COptimizer_RunAddPropagation(function);

        if (gArrayToRegisterEnabled) {
            COpt_ArrayToRegister();
            if (gArrayToRegisterChanged && gCOptimizerDumpEnabled) {
                COptimizer_DumpStage(function,
                                     "AFTER ARRAY => REGISTER TRANSFORM");
                COpt_ConstantPropagation();
                if (gConstantPropagationChanged) {
                    COpt_CopyPropagation(1);
                }
                COptimizer_DumpStage(function, "AFTER CONSTANT PROPAGATION 2");
            }
        }
    }

    COpt_ValueNumbering(1);
    if (gValueNumberingChanged) {
        COpt_CopyPropagation(1);
        if (gCOptimizerDumpEnabled) {
            COptimizer_DumpStage(function, "AFTER VALUE NUMBERING 2");
        }
    }

    if (gVectorArrayConversion && COpt_VectorArrayConversion()) {
        COpt_CopyPropagation(0);
        COpt_CopyPropagation(1);
        if (gCOptimizerDumpEnabled) {
            COptimizer_DumpStage(function, "AFTER VECTOR ARRAY CONVERSION");
        }
    }

    COpt_00522990();
    if (!gLoopCodeMotionEnabled) {
        return;
    }

    COpt_SetLoopCodeMotionMode(1);
    COpt_00521a10();
    COpt_00524bd0();
    COptimizer_DumpIfChanged(function, gCodeMotionChanged,
                             "AFTER CODE MOTION 2");

    COpt_ValueNumbering(1);
    if (gValueNumberingChanged) {
        COpt_CopyPropagation(1);
        if (gCOptimizerDumpEnabled) {
            COptimizer_DumpStage(function, "AFTER VALUE NUMBERING 3");
        }
    }
}
