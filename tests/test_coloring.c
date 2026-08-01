#include "mwcc/Coloring.h"
#include "mwcc/Registers.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

unsigned char gCOptimizerDumpEnabled;
unsigned char gColoringGuard_00584244;
unsigned char gHasAltivecFrame;
short gUsedVirtualRegistersVR;
short gUsedVirtualRegistersGPR;
short gUsedVirtualRegistersFPR;
short gColoringRegisterCount;
int gVirtualRegistersActive;

InterferenceNode** gInterferenceGraph;
ObjectList* gRegisterObjectList1;
ObjectList* gRegisterObjectList2;
PCodeBlock* gPCodeBlocks;
float gSpillScore_0056309c = 1000000.0F;
float gSpillScore_005630a0 = 1000000.0F;

static InterferenceNode gNodes[40];
static InterferenceNode* gNodePointers[40];
static int gUnexpectedAssert;
static unsigned int gTestColorMask;
static short gTestClaimedColor;
static int gResetClass;
static int gPreparedSpillCosts;
static int gRemovedInstructions;

static void Check(int condition, const char* message)
{
    if (!condition) {
        fprintf(stderr, "coloring test failed: %s\n", message);
        exit(1);
    }
}

RegisterInfo* Registers_GetInfo(CompilerObject* object)
{
    Check(object->register_info_2e != 0, "object register info");
    return object->register_info_2e;
}

char* Coloring_GetFunctionObject(PCodeFunction* function)
{
    return (char*) function;
}

void Coloring_Dump(const char* function_name, const char* stage)
{
    (void) function_name;
    (void) stage;
}

void Coloring_Error(int code, const char* register_class)
{
    (void) code;
    (void) register_class;
}

void Coloring_Assert(const char* file, int line)
{
    (void) file;
    (void) line;
    gUnexpectedAssert = 1;
}

void Registers_SetupVRs(void) {}
void Registers_SetupGPRs(void) {}
void Registers_SetupFPRs(void) {}
int Registers_AvailableVRs(void)
{
    return 32;
}
int Registers_AvailableGPRs(void)
{
    return 32;
}
int Registers_AvailableFPRs(void)
{
    return 32;
}

void SpillCode_BuildInterference(PCodeFunction* function, int reg_class,
                                 int register_count)
{
    (void) function;
    (void) reg_class;
    (void) register_count;
}

void SpillCode_00531800(int reg_class, int register_count)
{
    (void) reg_class;
    (void) register_count;
}

void SpillCode_00532790(int reg_class)
{
    (void) reg_class;
    gPreparedSpillCosts++;
}

void Coloring_ResetGPRColors(void)
{
    gResetClass = RegClass_GPR;
}

void Coloring_ResetFPRColors(void)
{
    gResetClass = RegClass_FPR;
}

void Coloring_ResetVRColors(void)
{
    gResetClass = RegClass_VR;
}
unsigned int Coloring_GPRColorMask(void)
{
    return gTestColorMask;
}
unsigned int Coloring_FPRColorMask(void)
{
    return gTestColorMask;
}
unsigned int Coloring_VRColorMask(void)
{
    return gTestColorMask;
}
short Coloring_ClaimGPRColor(void)
{
    return gTestClaimedColor;
}
short Coloring_ClaimFPRColor(void)
{
    return gTestClaimedColor;
}
short Coloring_ClaimVRColor(void)
{
    return gTestClaimedColor;
}

void PCode_RemoveRedundantInstruction(PCodeInstruction* instruction)
{
    (void) instruction;
    gRemovedInstructions++;
}

void Coloring_FreeIteration(void) {}
void StackFrame_CheckAltivec(void) {}

static void ResetGraph(void)
{
    int reg;

    memset(gNodes, 0, sizeof(gNodes));
    for (reg = 0; reg < 40; reg++) {
        gNodes[reg].virtual_register = (short) reg;
        gNodes[reg].physical_register = reg < 32 ? (short) reg : -1;
        gNodePointers[reg] = &gNodes[reg];
    }
    gInterferenceGraph = gNodePointers;
    gRegisterObjectList1 = 0;
    gRegisterObjectList2 = 0;
    gColoringGuard_00584244 = 0;
    gColoringRegisterCount = 40;
    gUnexpectedAssert = 0;
    gTestColorMask = 0;
    gTestClaimedColor = -1;
    gResetClass = -1;
    gPreparedSpillCosts = 0;
    gRemovedInstructions = 0;
    gPCodeBlocks = 0;
}

static void TestSimplifyAndSelect(void)
{
    InterferenceNode* stack;

    ResetGraph();
    gNodes[32].degree = 1;
    gNodes[32].neighbor_count = 1;
    gNodes[32].neighbors[0] = 33;
    gNodes[33].degree = 1;
    gNodes[33].neighbor_count = 1;
    gNodes[33].neighbors[0] = 32;
    stack = Coloring_SimplifyGraph(RegClass_GPR, 2, 34);
    Check(stack == &gNodes[33] && stack->next == &gNodes[32],
          "low-degree simplify stack");
    Check((gNodes[32].flags & Interference_Simplified) != 0,
          "first simplified node");
    Check((gNodes[33].flags & Interference_Simplified) != 0,
          "second simplified node");

    ResetGraph();
    gNodes[32].spill_cost = 10;
    gNodes[32].degree = 1;
    gNodes[32].neighbor_count = 1;
    gNodes[32].neighbors[0] = 33;
    gNodes[33].spill_cost = 2;
    gNodes[33].degree = 1;
    gNodes[33].neighbor_count = 1;
    gNodes[33].neighbors[0] = 32;
    stack = Coloring_SimplifyGraph(RegClass_GPR, 1, 34);
    Check(gPreparedSpillCosts == 1, "spill-cost preparation");
    Check(stack == &gNodes[32] && stack->next == &gNodes[33],
          "minimum spill-cost candidate");

    ResetGraph();
    gTestColorMask = 3;
    gNodes[32].neighbor_count = 1;
    gNodes[32].neighbors[0] = 0;
    Check(Coloring_SelectColors(RegClass_GPR, &gNodes[32]),
          "ordinary color selection");
    Check(gResetClass == RegClass_GPR, "GPR color reset");
    Check(gNodes[32].physical_register == 1, "lowest available color");

    ResetGraph();
    gTestClaimedColor = 5;
    Check(Coloring_SelectColors(RegClass_FPR, &gNodes[32]),
          "additional color claim");
    Check(gNodes[32].physical_register == 5, "claimed color");

    ResetGraph();
    Check(!Coloring_SelectColors(RegClass_VR, &gNodes[32]),
          "color exhaustion result");
    Check((gNodes[32].flags & Interference_Spilled) != 0,
          "color exhaustion spill flag");
}

static void TestCommitAssignments(void)
{
    typedef struct TestInstruction {
        PCodeInstruction instruction;
        PCodeOperand second_operand;
    } TestInstruction;

    CompilerObject primary_object;
    CompilerObject secondary_object;
    RegisterInfo primary_info;
    RegisterInfo secondary_info;
    TestInstruction storage;
    PCodeBlock block;

    ResetGraph();
    memset(&primary_object, 0, sizeof(primary_object));
    memset(&secondary_object, 0, sizeof(secondary_object));
    memset(&primary_info, 0, sizeof(primary_info));
    memset(&secondary_info, 0, sizeof(secondary_info));
    memset(&storage, 0, sizeof(storage));
    memset(&block, 0, sizeof(block));

    primary_object.register_info_2e = &primary_info;
    secondary_object.register_info_2e = &secondary_info;
    gNodes[32].object = &primary_object;
    gNodes[32].physical_register = 5;
    gNodes[33].object = &secondary_object;
    gNodes[33].physical_register = 32;
    gNodes[33].flags = Interference_Coalesced | Interference_SecondOfPair;

    storage.instruction.flags = 0x800;
    storage.instruction.operand_count = 2;
    storage.instruction.operands[0].kind = RegClass_GPR;
    storage.instruction.operands[0].reg = 32;
    storage.second_operand.kind = RegClass_GPR;
    storage.second_operand.reg = 32;
    block.instructions = &storage.instruction;
    gPCodeBlocks = &block;

    Coloring_CommitAssignments(RegClass_GPR, 34);
    Check(storage.instruction.operands[0].reg == 5, "primary PCode rewrite");
    Check(storage.second_operand.reg == 5, "second PCode rewrite");
    Check(gRemovedInstructions == 1, "redundant instruction cleanup");
    Check(primary_info.physical_register == 5, "primary object color");
    Check(secondary_info.secondary_register == 5, "secondary object color");
}

static void TestClassSetup(void)
{
    CompilerObject vector_object;
    CompilerObject fpr_object;
    CompilerObject gpr_object;
    CompilerType gpr_type;
    RegisterInfo vector_info;
    RegisterInfo fpr_info;
    RegisterInfo gpr_info;
    ObjectList vector_item;
    ObjectList fpr_item;
    ObjectList gpr_item;
    int reg;

    memset(&vector_object, 0, sizeof(vector_object));
    memset(&fpr_object, 0, sizeof(fpr_object));
    memset(&gpr_object, 0, sizeof(gpr_object));
    memset(&gpr_type, 0, sizeof(gpr_type));
    memset(&vector_info, 0, sizeof(vector_info));
    memset(&fpr_info, 0, sizeof(fpr_info));
    memset(&gpr_info, 0, sizeof(gpr_info));

    vector_info.physical_register = 3;
    vector_info.is_vector = 1;
    vector_object.register_info_2e = &vector_info;
    vector_item.next = 0;
    vector_item.object = &vector_object;

    ResetGraph();
    gRegisterObjectList1 = &vector_item;
    Coloring_SetupVRs();
    for (reg = 0; reg < 32; reg++) {
        Check(gNodes[reg].physical_register == reg, "initial physical color");
    }
    Check(gNodes[3].object == &vector_object, "precolored VR object");

    fpr_info.physical_register = 4;
    fpr_info.is_fpr = 1;
    fpr_object.register_info_2e = &fpr_info;
    fpr_item.next = 0;
    fpr_item.object = &fpr_object;

    ResetGraph();
    gRegisterObjectList2 = &fpr_item;
    Coloring_SetupFPRs();
    Check(gNodes[4].object == &fpr_object, "precolored FPR object");
    Check(!gUnexpectedAssert, "ordinary FPR setup assertion");

    gpr_type.kind = 1;
    gpr_type.size = 8;
    gpr_info.physical_register = 5;
    gpr_info.secondary_register = 6;
    gpr_object.type = &gpr_type;
    gpr_object.register_info_2e = &gpr_info;
    gpr_item.next = 0;
    gpr_item.object = &gpr_object;

    ResetGraph();
    gRegisterObjectList1 = &gpr_item;
    Coloring_SetupGPRs();
    Check(gNodes[5].object == &gpr_object, "precolored primary GPR");
    Check((gNodes[5].flags & 0x20) != 0, "primary GPR pair flag");
    Check(gNodes[6].object == &gpr_object, "precolored secondary GPR");
    Check((gNodes[6].flags & 0x10) != 0, "secondary GPR pair flag");
}

int main(void)
{
    TestClassSetup();
    TestSimplifyAndSelect();
    TestCommitAssignments();
    puts("coloring model tests passed");
    return 0;
}
