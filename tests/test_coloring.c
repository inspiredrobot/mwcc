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

static InterferenceNode gNodes[32];
static InterferenceNode* gNodePointers[32];
static int gUnexpectedAssert;

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

int* Coloring_004ce400(int reg_class, int available, int register_count)
{
    (void) reg_class;
    (void) available;
    (void) register_count;
    return 0;
}

int Coloring_004ce2d0(int reg_class, int* graph)
{
    (void) reg_class;
    (void) graph;
    return 1;
}

void Coloring_004ce1a0(int reg_class, int register_count)
{
    (void) reg_class;
    (void) register_count;
}

void SpillCode_00531800(int reg_class, int register_count)
{
    (void) reg_class;
    (void) register_count;
}

void Coloring_FreeIteration(void) {}
void StackFrame_CheckAltivec(void) {}

static void ResetGraph(void)
{
    int reg;

    memset(gNodes, 0, sizeof(gNodes));
    for (reg = 0; reg < 32; reg++) {
        gNodes[reg].physical_register = -1;
        gNodePointers[reg] = &gNodes[reg];
    }
    gInterferenceGraph = gNodePointers;
    gRegisterObjectList1 = 0;
    gRegisterObjectList2 = 0;
    gColoringGuard_00584244 = 0;
    gUnexpectedAssert = 0;
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
    puts("coloring model tests passed");
    return 0;
}
