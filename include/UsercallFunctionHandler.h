#pragma once
#ifdef COMPILE_MOD
#include "bool.h"
#include <stdint.h>

typedef enum registers
{
	noret,

	rEAX, rEBX, rECX, rEDX,
	rAX,  rBX,  rCX,  rDX,
	rAL,  rBL,  rCL,  rDL,
	rAH,  rBH,  rCH,  rDH,

	rESI, rEDI, rEBP,
	rSI, rDI, rBP,

	stack1,
	stack2,
	stack4,
	rst0
} registers;

void const GenerateUsercallHook(void* func, registers ret, unsigned short stdcallSize, intptr_t address, bool preserveABCD, int argCount, ...);

void* const GenerateUsercallWrapper(registers ret, unsigned short stdcallSize, intptr_t address, bool preserveABCD, int argCount, ...);

void* const CopyFunction(intptr_t address, int bytes);

#endif