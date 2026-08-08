#include "mwcc/Registers.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

unsigned char gColoringGuard_00584244;
int gUseVirtualRegisterNumbers_00587f00;
short gUsedVirtualRegistersVR;
short gUsedVirtualRegistersGPR;
short gUsedVirtualRegistersFPR;

short gGPRCoalesceFirst;
short gFPRCoalesceFirst;
short gFPRCoalesceLast;
short gGPRCoalesceLast;
short gVRCoalesceFirst;
short gVRCoalesceLast;
short gFPRCounterCheckpoint;
short gGPRCounterCheckpoint;
short gVRCounterCheckpoint;
short gInitialObjectGPRLast;
short gInitialObjectFPRLast;
short gInitialObjectVRLast;

unsigned char gUsedPhysicalGPR[32];
unsigned char gUsedPhysicalFPR[32];
unsigned char gUsedPhysicalVR[32];
short gColoringSaveSpan_00581370;
unsigned char gColoringPhysicalUse_00581372[32];

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
    gGPRCoalesceFirst = 0;
    gFPRCoalesceFirst = 0;
    gFPRCoalesceLast = 0;
    gGPRCoalesceLast = 0;
    gVRCoalesceFirst = 0;
    gVRCoalesceLast = 0;
    gFPRCounterCheckpoint = 0;
    gGPRCounterCheckpoint = 0;
    gVRCounterCheckpoint = 0;
    gInitialObjectGPRLast = 0;
    gInitialObjectFPRLast = 0;
    gInitialObjectVRLast = 0;
    memset(gUsedPhysicalGPR, 0, sizeof(gUsedPhysicalGPR));
    memset(gUsedPhysicalFPR, 0, sizeof(gUsedPhysicalFPR));
    memset(gUsedPhysicalVR, 0, sizeof(gUsedPhysicalVR));
    gColoringSaveSpan_00581370 = 0;
    memset(gColoringPhysicalUse_00581372, 0,
           sizeof(gColoringPhysicalUse_00581372));
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

static void TestColoringState(void)
{
    unsigned int expected_mask;

    ResetState();
    gUsedPhysicalGPR[2] = 1;
    gGPRSaveSpan = 7;
    Registers_SetupGPRs();
    Check(gColoringSaveSpan_00581370 == 7, "saved GPR span snapshot");
    Check(gColoringPhysicalUse_00581372[2] == 1, "saved GPR use snapshot");

    memset(gUsedPhysicalGPR, 1, sizeof(gUsedPhysicalGPR));
    gGPRSaveSpan = 0;
    Coloring_ResetGPRColors();
    Check(gGPRSaveSpan == 7, "restored GPR span");
    Check(gUsedPhysicalGPR[2] == 1 && gUsedPhysicalGPR[3] == 0,
          "restored GPR use table");

    expected_mask = ((1U << 13) - 1U) & ~(1U << 2);
    Check(Coloring_GPRColorMask() == expected_mask, "GPR color mask");
    Check(Registers_AvailableGPRs() == 31, "available GPR count");
    Check(Coloring_ClaimGPRColor() == 31, "claim highest saved GPR");
    Check(gUsedPhysicalGPR[31] == 1, "claimed GPR use bit");

    ResetState();
    gUsedPhysicalFPR[0] = 1;
    Check(Coloring_FPRColorMask() == ((1U << 14) - 2U), "FPR color mask");
    Check(Coloring_ClaimFPRColor() == 31, "claim highest saved FPR");

    ResetState();
    gUsedPhysicalVR[19] = 1;
    Check(Coloring_VRColorMask() == ((1U << 19) - 1U), "VR color mask");
    Check(Coloring_ClaimVRColor() == 31, "claim highest saved VR");
}

static void TestCoalesceWindow(void)
{
    ResetState();
    gUsedVirtualRegistersGPR = 40;
    gUsedVirtualRegistersFPR = 50;
    gUsedVirtualRegistersVR = 60;
    Registers_BeginCoalesceWindow();
    Check(gGPRCoalesceFirst == 40 && gGPRCoalesceLast == 40,
          "begin GPR coalesce window");
    Check(gFPRCoalesceFirst == 50 && gFPRCoalesceLast == 50,
          "begin FPR coalesce window");
    Check(gVRCoalesceFirst == 60 && gVRCoalesceLast == 60,
          "begin VR coalesce window");

    Registers_SnapshotInitialObjectRange();
    Check(gInitialObjectGPRLast == 39, "snapshot initial GPR object range");
    Check(gInitialObjectFPRLast == 49, "snapshot initial FPR object range");
    Check(gInitialObjectVRLast == 59, "snapshot initial VR object range");

    gUsedVirtualRegistersGPR = 44;
    gUsedVirtualRegistersFPR = 55;
    gUsedVirtualRegistersVR = 66;
    Registers_CheckpointCoalesceWindow();
    Check(gGPRCoalesceLast == 44 && gGPRCounterCheckpoint == 44,
          "checkpoint GPR coalesce window");
    Check(gFPRCoalesceLast == 55 && gFPRCounterCheckpoint == 55,
          "checkpoint FPR coalesce window");
    Check(gVRCoalesceLast == 66 && gVRCounterCheckpoint == 66,
          "checkpoint VR coalesce window");

    gUsedVirtualRegistersGPR = 42;
    gUsedVirtualRegistersFPR = 57;
    Registers_CloseCoalesceWindow();
    Check(gUsedVirtualRegistersGPR == 44, "close restores GPR high watermark");
    Check(gFPRCoalesceLast == 57, "close extends FPR high watermark");

    gUsedVirtualRegistersGPR = 257;
    gUsedVirtualRegistersFPR = 58;
    gUsedVirtualRegistersVR = 67;
    gUseVirtualRegisterNumbers_00587f00 = 0;
    Registers_UpdateCoalesceWindow();
    Check(gGPRCoalesceLast == 257 && gUsedVirtualRegistersGPR == 44,
          "update records and rolls back large GPR counter");
    Check(gFPRCoalesceLast == 58 && gVRCoalesceLast == 67,
          "update extends class high watermarks");
}

int main(void)
{
    TestExplicitBindings();
    TestAutomaticAllocation();
    TestColoringState();
    TestCoalesceWindow();
    puts("register model tests passed");
    return 0;
}
