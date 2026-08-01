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

#define COPT_DUMP(function, stage)                                            \
    COptimizer_Dump(COptimizer_GetFunctionObject(function) + 10, (stage))

#define COPT_DUMP_IF_CHANGED(function, changed, stage)                        \
    do {                                                                      \
        if ((changed) && gCOptimizerDumpEnabled) {                            \
            COPT_DUMP((function), (stage));                                   \
        }                                                                     \
    } while (0)

#define COPT_RUN_COPY_PROPAGATION(function, mode)                             \
    do {                                                                      \
        COpt_CopyPropagation(mode);                                           \
        COPT_DUMP_IF_CHANGED((function), gCopyPropagationChanged,             \
                             "AFTER COPY PROPAGATION");                       \
    } while (0)

#define COPT_RUN_ADD_PROPAGATION(function)                                    \
    do {                                                                      \
        COpt_AddPropagation();                                                \
        COPT_DUMP_IF_CHANGED((function), gAddPropagationChanged,              \
                             "AFTER ADD PROPAGATION");                        \
    } while (0)

#define COPT_RUN_LOOP_PASSES(function)                                        \
    do {                                                                      \
        COpt_00522990();                                                      \
        if (gLoopCodeMotionEnabled) {                                         \
            COpt_SetLoopCodeMotionMode(1);                                    \
            COpt_00521a10();                                                  \
            COpt_00524bd0();                                                  \
            COpt_SetLoopCodeMotionMode(0);                                    \
            COpt_00521d10(gLoopCodeMotionEnabled);                            \
            COPT_DUMP_IF_CHANGED((function), gCodeMotionChanged,              \
                                 "AFTER CODE MOTION");                        \
            COpt_StrengthReduction();                                         \
            if (gStrengthReductionChanged) {                                  \
                COpt_CopyPropagation(1);                                      \
                if (gCOptimizerDumpEnabled) {                                 \
                    COPT_DUMP((function), "AFTER STRENGTH REDUCTION");        \
                }                                                             \
            }                                                                 \
            COpt_LoopTransformations();                                       \
            if (gLoopTransformChanged) {                                      \
                COpt_CopyPropagation(1);                                      \
                COpt_AddPropagation();                                        \
                if (gCOptimizerDumpEnabled) {                                 \
                    COPT_DUMP((function), "AFTER LOOP TRANSFORMATIONS");      \
                }                                                             \
            }                                                                 \
        }                                                                     \
        if (!gCopyPropagationChanged) {                                       \
            COPT_RUN_COPY_PROPAGATION((function), 1);                         \
        }                                                                     \
    } while (0)

/* 0x004c4430; functionally equivalent; binary match unmeasured. */
void COptimizer_Optimize(PCodeFunction* function)
{
    if (gCOptimizerDumpEnabled) {
        COPT_DUMP(function, "BEFORE GLOBAL OPTIMIZATION");
    }

    if (gOptimizationLevel == 2 ||
        (gRunLevel2Pipeline && gOptimizationLevel > 2))
    {
        COpt_ValueNumbering(1);
        COPT_DUMP_IF_CHANGED(function, gValueNumberingChanged, "AFTER CSE");
        COPT_RUN_COPY_PROPAGATION(function, 1);
        COPT_RUN_ADD_PROPAGATION(function);
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
    COPT_DUMP_IF_CHANGED(function, gValueNumberingChanged,
                         "AFTER VALUE NUMBERING");
    COPT_RUN_COPY_PROPAGATION(function, 0);

    gCopyPropagationChanged = 0;
    COPT_RUN_ADD_PROPAGATION(function);
    COPT_RUN_LOOP_PASSES(function);

    COpt_ConstantPropagation();
    if (gConstantPropagationChanged) {
        if (gCOptimizerDumpEnabled) {
            COPT_DUMP(function, "AFTER CONSTANT PROPAGATION");
        }
        COpt_LoadDeletion();
        COPT_DUMP_IF_CHANGED(function, gLoadDeletionChanged,
                             "AFTER LOAD DELETION");
        COPT_RUN_ADD_PROPAGATION(function);
    }

    COpt_ValueNumbering(1);
    if (gValueNumberingChanged) {
        COpt_CopyPropagation(1);
        if (gCOptimizerDumpEnabled) {
            COPT_DUMP(function, "AFTER VALUE NUMBERING 2");
        }
    }
}

/* 0x004c4530; functionally equivalent; binary match unmeasured. */
void COptimizer_Level4(PCodeFunction* function)
{
    COpt_ValueNumbering(0);
    COPT_DUMP_IF_CHANGED(function, gValueNumberingChanged,
                         "AFTER VALUE NUMBERING");
    COPT_RUN_COPY_PROPAGATION(function, 0);

    gCopyPropagationChanged = 0;
    COPT_RUN_ADD_PROPAGATION(function);
    COPT_RUN_LOOP_PASSES(function);

    COpt_ConstantPropagation();
    if (gConstantPropagationChanged) {
        if (gCOptimizerDumpEnabled) {
            COPT_DUMP(function, "AFTER CONSTANT PROPAGATION");
        }
        COpt_LoadDeletion();
        COPT_DUMP_IF_CHANGED(function, gLoadDeletionChanged,
                             "AFTER LOAD DELETATION");

        if (gConstantPropagationChanged) {
            COpt_CopyPropagation(1);
        }
        COPT_RUN_ADD_PROPAGATION(function);

        if (gArrayToRegisterEnabled) {
            COpt_ArrayToRegister();
            if (gArrayToRegisterChanged && gCOptimizerDumpEnabled) {
                COPT_DUMP(function, "AFTER ARRAY => REGISTER TRANSFORM");
                COpt_ConstantPropagation();
                if (gConstantPropagationChanged) {
                    COpt_CopyPropagation(1);
                }
                COPT_DUMP(function, "AFTER CONSTANT PROPAGATION 2");
            }
        }
    }

    COpt_ValueNumbering(1);
    if (gValueNumberingChanged) {
        COpt_CopyPropagation(1);
        if (gCOptimizerDumpEnabled) {
            COPT_DUMP(function, "AFTER VALUE NUMBERING 2");
        }
    }

    if (gVectorArrayConversion && COpt_VectorArrayConversion()) {
        COpt_CopyPropagation(0);
        COpt_CopyPropagation(1);
        if (gCOptimizerDumpEnabled) {
            COPT_DUMP(function, "AFTER VECTOR ARRAY CONVERSION");
        }
    }

    COpt_00522990();
    if (!gLoopCodeMotionEnabled) {
        return;
    }

    COpt_SetLoopCodeMotionMode(1);
    COpt_00521a10();
    COpt_00524bd0();
    COPT_DUMP_IF_CHANGED(function, gCodeMotionChanged, "AFTER CODE MOTION 2");

    COpt_ValueNumbering(1);
    if (gValueNumberingChanged) {
        COpt_CopyPropagation(1);
        if (gCOptimizerDumpEnabled) {
            COPT_DUMP(function, "AFTER VALUE NUMBERING 3");
        }
    }
}
