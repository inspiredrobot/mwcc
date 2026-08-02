#include "mwcc/Operands.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

unsigned char gOperandsDebug;
short gUsedVirtualRegistersFPR;

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

void Operands_Assert(const char* file, int line)
{
    (void) file;
    gAssertLine = line;
}

void Operands_DebugType(int code)
{
    gDebugCode = code;
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

void PCodeUtilities_EmitInstruction(int opcode, ...)
{
    va_list arguments;

    va_start(arguments, opcode);
    gOpcode = (short) opcode;
    gDestination = (short) va_arg(arguments, int);
    gBase = (short) va_arg(arguments, int);
    gIndex = (short) va_arg(arguments, int);
    va_end(arguments);
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
}

int main(void)
{
    CompilerType type;
    CompilerObject object;
    Operand operand;

    memset(&type, 0, sizeof(type));
    memset(&object, 0, sizeof(object));
    memset(&operand, 0, sizeof(operand));

    Reset();
    gUsedVirtualRegistersFPR = 107;
    type.size = 8;
    operand.kind = 9;
    operand.reg = 41;
    operand.displacement = -12;
    operand.flags_0a = 0x1234;
    operand.object = &object;
    Operands_ForceFPR(&operand, &type, 0);
    Check(operand.kind == 5 && operand.reg == 107, "direct FPR result");
    Check(gUsedVirtualRegistersFPR == 108, "direct FPR allocation");
    Check(gOpcode == 0x92, "LFD selection");
    Check(gDestination == 107 && gBase == 41, "LFD registers");
    Check(gObject == &object && gDisplacement == -12, "LFD address");
    Check(gPropagatedFlags == 0x1234, "direct flags");

    Reset();
    type.size = 4;
    operand.kind = 10;
    operand.reg = 50;
    operand.secondary_reg = 51;
    operand.flags_0a = 0xabcd;
    Operands_ForceFPR(&operand, &type, 9);
    Check(operand.kind == 5 && operand.reg == 9, "indexed FPR result");
    Check(gUsedVirtualRegistersFPR == 108, "requested FPR reuse");
    Check(gOpcode == 0x90, "LFSX selection");
    Check(gDestination == 9 && gBase == 50 && gIndex == 51, "LFSX registers");
    Check(gPropagatedFlags == 0xabcd, "indexed flags");

    Reset();
    operand.kind = 5;
    operand.reg = 23;
    Operands_ForceFPR(&operand, &type, 0);
    Check(operand.reg == 23 && gOpcode == 0, "existing FPR reuse");
    Check(gNormalizeCount == 1, "operand normalization");

    Reset();
    gOperandsDebug = 1;
    type.kind = 2;
    operand.kind = 6;
    Operands_ForceFPR(&operand, &type, 0);
    Check(gDebugCode == 0x84, "debug type check");
    Check(gAssertLine == 0x320, "invalid operand assertion");

    puts("operand model tests passed");
    return 0;
}
