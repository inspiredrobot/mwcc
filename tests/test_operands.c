#include "mwcc/Operands.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct Emission {
    short opcode;
    int argument_count;
    int arguments[5];
} Emission;

unsigned char gOperandsDebug;
signed char gOptimizationLevel;
short gUsedVirtualRegistersFPR;
short gUsedVirtualRegistersGPR;

static int gNormalizeCount;
static int gDebugCode;
static int gAssertLine;
static short gOpcode;
static short gDestination;
static short gBase;
static short gIndex;
static CompilerObject* gObject;
static int gDisplacement;
static unsigned int gPropagatedFlags;
static int gUnsignedType;
static int gPairCount;
static short gPairFirst;
static short gPairSecond;
static int gAddressEmitCount;
static Emission gEmissions[8];
static int gEmissionCount;

void Operands_Assert(const char* file, int line)
{
    (void) file;
    gAssertLine = line;
}

void Operands_DebugType(int code)
{
    gDebugCode = code;
}

unsigned char Type_IsUnsigned(CompilerType* type)
{
    (void) type;
    return (unsigned char) gUnsignedType;
}

void Operands_ForceGPRPair(Operand* operand, CompilerType* type,
                           short requested_register, short requested_second)
{
    (void) operand;
    (void) type;
    gPairCount++;
    gPairFirst = requested_register;
    gPairSecond = requested_second;
}

void Operands_Normalize(Operand* operand)
{
    (void) operand;
    gNormalizeCount++;
}

void Operands_EmitMemoryInstruction(short opcode, short destination,
                                    short base, CompilerObject* object,
                                    int displacement)
{
    gOpcode = opcode;
    gDestination = destination;
    gBase = base;
    gObject = object;
    gDisplacement = displacement;
}

void PCodeUtilities_EmitAddress(short destination, short base,
                                CompilerObject* object, short displacement)
{
    gAddressEmitCount++;
    gDestination = destination;
    gBase = base;
    gObject = object;
    gDisplacement = displacement;
}

static int PCodeArgumentCount(int opcode)
{
    switch (opcode) {
    case 0x3f:
        return 4;
    case 0x67:
        return 5;
    case 0x82:
        return 1;
    case 0x89:
        return 2;
    case 0x8a:
        return 3;
    default:
        return 3;
    }
}

void PCodeUtilities_EmitInstruction(int opcode, ...)
{
    Emission* emission;
    va_list arguments;
    int index;

    emission = &gEmissions[gEmissionCount++];
    emission->opcode = (short) opcode;
    emission->argument_count = PCodeArgumentCount(opcode);
    va_start(arguments, opcode);
    for (index = 0; index < emission->argument_count; index++) {
        emission->arguments[index] = va_arg(arguments, int);
    }
    va_end(arguments);

    gOpcode = (short) opcode;
    gDestination = (short) emission->arguments[0];
    if (emission->argument_count > 1) {
        gBase = (short) emission->arguments[1];
    }
    if (emission->argument_count > 2) {
        gIndex = (short) emission->arguments[2];
    }
}

void Operands_PropagateFlags(unsigned int flags)
{
    gPropagatedFlags = flags;
}

static void Check(int condition, const char* message)
{
    if (!condition) {
        fprintf(stderr, "FAIL: %s\n", message);
        exit(1);
    }
}

static void Reset(void)
{
    gOperandsDebug = 0;
    gOptimizationLevel = 0;
    gNormalizeCount = 0;
    gDebugCode = 0;
    gAssertLine = 0;
    gOpcode = 0;
    gDestination = 0;
    gBase = 0;
    gIndex = 0;
    gObject = 0;
    gDisplacement = 0;
    gPropagatedFlags = 0;
    gUnsignedType = 0;
    gPairCount = 0;
    gPairFirst = 0;
    gPairSecond = 0;
    gAddressEmitCount = 0;
    memset(gEmissions, 0, sizeof(gEmissions));
    gEmissionCount = 0;
}

static void TestFPRs(CompilerType* type, CompilerObject* object,
                     Operand* operand)
{
    Reset();
    memset(operand, 0, sizeof(*operand));
    gUsedVirtualRegistersFPR = 107;
    type->size = 8;
    operand->kind = 9;
    operand->reg = 41;
    operand->displacement = -12;
    operand->flags_0a = 0x1234;
    operand->object = object;
    Operands_ForceFPR(operand, type, 0);
    Check(operand->kind == 5 && operand->reg == 107, "direct FPR result");
    Check(gUsedVirtualRegistersFPR == 108, "direct FPR allocation");
    Check(gOpcode == 0x92, "LFD selection");
    Check(gDestination == 107 && gBase == 41, "LFD registers");
    Check(gObject == object && gDisplacement == -12, "LFD address");
    Check(gPropagatedFlags == 0x1234, "direct flags");

    Reset();
    memset(operand, 0, sizeof(*operand));
    type->size = 4;
    operand->kind = 10;
    operand->reg = 50;
    operand->secondary_reg = 51;
    operand->flags_0a = 0xabcd;
    Operands_ForceFPR(operand, type, 9);
    Check(operand->kind == 5 && operand->reg == 9, "indexed FPR result");
    Check(gUsedVirtualRegistersFPR == 108, "requested FPR reuse");
    Check(gOpcode == 0x90, "LFSX selection");
    Check(gDestination == 9 && gBase == 50 && gIndex == 51, "LFSX registers");
    Check(gPropagatedFlags == 0xabcd, "indexed flags");

    Reset();
    memset(operand, 0, sizeof(*operand));
    operand->kind = 5;
    operand->reg = 23;
    Operands_ForceFPR(operand, type, 0);
    Check(operand->reg == 23 && gOpcode == 0, "existing FPR reuse");
    Check(gNormalizeCount == 1, "FPR operand normalization");

    Reset();
    memset(operand, 0, sizeof(*operand));
    gOperandsDebug = 1;
    type->kind = 2;
    operand->kind = 6;
    Operands_ForceFPR(operand, type, 0);
    Check(gDebugCode == 0x84, "debug type check");
    Check(gAssertLine == 0x320, "invalid FPR operand assertion");
}

static void TestGPRMemory(CompilerType* type, CompilerObject* object,
                          Operand* operand)
{
    Reset();
    memset(operand, 0, sizeof(*operand));
    gUsedVirtualRegistersGPR = 200;
    type->kind = 11;
    type->size = 4;
    operand->kind = 9;
    operand->reg = 31;
    operand->displacement = -20;
    operand->flags_0a = 0x4567;
    operand->object = object;
    Operands_ForceGPR(operand, type, 0);
    Check(operand->kind == 0 && operand->reg == 200, "direct GPR result");
    Check(gUsedVirtualRegistersGPR == 201, "direct GPR allocation");
    Check(gOpcode == 0x22, "LWZ selection");
    Check(gDestination == 200 && gBase == 31, "LWZ registers");
    Check(gObject == object && gDisplacement == -20, "LWZ address");
    Check(gPropagatedFlags == 0x4567, "GPR direct flags");

    Reset();
    memset(operand, 0, sizeof(*operand));
    type->kind = 1;
    type->size = 2;
    gUnsignedType = 1;
    operand->kind = 10;
    operand->reg = 40;
    operand->secondary_reg = 41;
    operand->flags_0a = 0x89ab;
    Operands_ForceGPR(operand, type, 17);
    Check(operand->kind == 0 && operand->reg == 17, "indexed GPR result");
    Check(gOpcode == 0x1b, "LHZX selection");
    Check(gDestination == 17 && gBase == 40 && gIndex == 41, "LHZX registers");
    Check(gPropagatedFlags == 0x89ab, "GPR indexed flags");
}

static void TestGPRForms(CompilerType* type, CompilerObject* object,
                         Operand* operand)
{
    Reset();
    memset(operand, 0, sizeof(*operand));
    gUsedVirtualRegistersGPR = 300;
    type->kind = 11;
    type->size = 4;
    operand->kind = 1;
    operand->reg = 2;
    operand->object = object;
    operand->displacement = 12;
    Operands_ForceGPR(operand, type, 0);
    Check(gAddressEmitCount == 1, "address emission");
    Check(gDestination == 300 && gBase == 2 && gObject == object,
          "address operands");

    Reset();
    memset(operand, 0, sizeof(*operand));
    operand->kind = 4;
    operand->immediate = -123;
    Operands_ForceGPR(operand, type, 12);
    Check(gEmissionCount == 1 && gEmissions[0].opcode == 0x89,
          "small immediate LI");
    Check(gEmissions[0].arguments[0] == 12 &&
              gEmissions[0].arguments[1] == -123,
          "LI operands");

    Reset();
    memset(operand, 0, sizeof(*operand));
    gUsedVirtualRegistersGPR = 400;
    gOptimizationLevel = 2;
    operand->kind = 4;
    operand->immediate = 0x12348000;
    Operands_ForceGPR(operand, type, 0);
    Check(gUsedVirtualRegistersGPR == 402, "large immediate temporaries");
    Check(gEmissionCount == 2 && gEmissions[0].opcode == 0x8a &&
              gEmissions[1].opcode == 0x3f,
          "large immediate instructions");
    Check(gEmissions[0].arguments[0] == 401 &&
              gEmissions[0].arguments[2] == 0x1235,
          "adjusted high immediate");
    Check(gEmissions[1].arguments[0] == 400 &&
              gEmissions[1].arguments[1] == 401 &&
              gEmissions[1].arguments[3] == -32768,
          "low immediate addition");

    Reset();
    memset(operand, 0, sizeof(*operand));
    operand->kind = 7;
    operand->reg = 3;
    operand->secondary_reg = 0x18;
    Operands_ForceGPR(operand, type, 25);
    Check(gEmissionCount == 3 && gEmissions[0].opcode == 0x82 &&
              gEmissions[1].opcode == 0x67 && gEmissions[2].opcode == 0x5a,
          "inverted condition instructions");
    Check(gEmissions[1].arguments[2] == 15, "condition-register bit position");
}

static void TestGPRPair(CompilerType* type, Operand* operand)
{
    Reset();
    memset(operand, 0, sizeof(*operand));
    type->kind = 1;
    type->size = 8;
    Operands_ForceGPR(operand, type, 33);
    Check(gPairCount == 1 && gPairFirst == 33 && gPairSecond == 0,
          "GPR pair delegation");
    Check(gNormalizeCount == 0, "pair delegates before normalization");
}

int main(void)
{
    CompilerType type;
    CompilerObject object;
    Operand operand;

    memset(&type, 0, sizeof(type));
    memset(&object, 0, sizeof(object));
    memset(&operand, 0, sizeof(operand));

    TestFPRs(&type, &object, &operand);
    TestGPRMemory(&type, &object, &operand);
    TestGPRForms(&type, &object, &operand);
    TestGPRPair(&type, &operand);

    puts("operand model tests passed");
    return 0;
}
