#include "mwcc/Registers.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

unsigned char gColoringGuard_00584244;
int gUseVirtualRegisterNumbers_00587f00;
short gUsedVirtualRegistersVR;
short gUsedVirtualRegistersGPR;
short gUsedVirtualRegistersFPR;

unsigned char gUsedPhysicalGPR[32];
unsigned char gUsedPhysicalFPR[32];
unsigned char gUsedPhysicalVR[32];

short gVRSaveSpan;
short gFPRSaveSpan;
short gGPRSaveSpan;
short gAvailableSavedFPRs;
short gAvailableSavedGPRs;
short gAvailableSavedVRs;

static RegisterInfo gRegisterInfoPool[16];
static int gRegisterInfoCount;

static void Check(int condition, const char* message)
{
    if (!condition) {
        fprintf(stderr, "register test failed: %s\n", message);
        exit(1);
    }
}

void* Registers_Allocate(unsigned int size)
{
    Check(size == sizeof(RegisterInfo), "allocation size");
    Check(gRegisterInfoCount < 16, "allocation pool");
    return &gRegisterInfoPool[gRegisterInfoCount++];
}

void Registers_Zero(void* data, unsigned int size)
{
    memset(data, 0, size);
}

void Registers_Assert(const char* file, int line)
{
    fprintf(stderr, "unexpected compiler assertion: %s:%d\n", file, line);
    exit(1);
}

static void ResetState(void)
{
    gColoringGuard_00584244 = 0;
    gUseVirtualRegisterNumbers_00587f00 = 0;
    gUsedVirtualRegistersVR = 0;
    gUsedVirtualRegistersGPR = 0;
    gUsedVirtualRegistersFPR = 0;
    memset(gUsedPhysicalGPR, 0, sizeof(gUsedPhysicalGPR));
    memset(gUsedPhysicalFPR, 0, sizeof(gUsedPhysicalFPR));
    memset(gUsedPhysicalVR, 0, sizeof(gUsedPhysicalVR));
    gVRSaveSpan = 0;
    gFPRSaveSpan = 0;
    gGPRSaveSpan = 0;
    gAvailableSavedFPRs = 0;
    gAvailableSavedGPRs = 0;
    gAvailableSavedVRs = 0;
    memset(gRegisterInfoPool, 0, sizeof(gRegisterInfoPool));
    gRegisterInfoCount = 0;
}

static void TestExplicitBindings(void)
{
    CompilerObject object;

    ResetState();
    memset(&object, 0, sizeof(object));
    Registers_BindVR(&object, 31);
    Check(gUsedPhysicalVR[31] == 1, "VR use bit");
    Check(gVRSaveSpan == 1, "VR save span");
    Check(gAvailableSavedVRs == 11, "available saved VRs");
    Check(object.register_info_2e->physical_register == 31, "bound VR");
    Check(object.register_info_2e->is_vector == 1, "VR class flag");

    Registers_BindVR(0, 20);
    Check(gVRSaveSpan == 12, "expanded VR save span");
    Check(gAvailableSavedVRs == 10, "updated saved VR count");

    ResetState();
    memset(&object, 0, sizeof(object));
    Registers_BindFPR(&object, 31);
    Registers_BindFPR(0, 14);
    Check(gFPRSaveSpan == 18, "FPR save span");
    Check(gAvailableSavedFPRs == 13, "available saved FPRs");
    Check(object.register_info_2e->is_fpr == 1, "FPR class flag");

    ResetState();
    memset(&object, 0, sizeof(object));
    Registers_BindGPR(&object, 31);
    Check(gGPRSaveSpan == 1, "GPR save span");
    Check(gAvailableSavedGPRs == 15, "available saved GPRs");
}

static void TestAutomaticAllocation(void)
{
    CompilerObject object;
    CompilerType type;

    ResetState();
    memset(&object, 0, sizeof(object));
    Registers_AllocateVR(&object);
    Check(object.register_info_2e->physical_register == 31,
          "physical VR fallback");

    ResetState();
    memset(&object, 0, sizeof(object));
    gUseVirtualRegisterNumbers_00587f00 = 1;
    gUsedVirtualRegistersVR = 40;
    Registers_AllocateVR(&object);
    Check(object.register_info_2e->physical_register == 40,
          "virtual VR number");
    Check(gUsedVirtualRegistersVR == 41, "virtual VR counter");

    ResetState();
    memset(&object, 0, sizeof(object));
    gUseVirtualRegisterNumbers_00587f00 = 1;
    gUsedVirtualRegistersGPR = 40;
    Registers_AllocateGPRPair(&object);
    Check(object.register_info_2e->physical_register == 40,
          "paired GPR first");
    Check(object.register_info_2e->secondary_register == 41,
          "paired GPR second");
    Check(gUsedVirtualRegistersGPR == 42, "paired GPR counter");

    ResetState();
    memset(&object, 0, sizeof(object));
    type.kind = 2;
    object.type = &type;
    gColoringGuard_00584244 = 1;
    gUseVirtualRegisterNumbers_00587f00 = 1;
    gUsedVirtualRegistersGPR = 40;
    Registers_AllocateGPR(&object);
    Check(object.register_info_2e->is_fpr == 1,
          "type-directed FPR classification");
}

int main(void)
{
    TestExplicitBindings();
    TestAutomaticAllocation();
    puts("register model tests passed");
    return 0;
}
