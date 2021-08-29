#ifdef COMPILE_MOD
#include "UsercallFunctionHandler.h"
#include "MemAccess.h"
#include <cassert>

char* curpg = NULL;
int pgoff = 0;
int pgsz = -1;
char* AllocateCode(int sz)
{
	if (pgsz == -1)
	{
		SYSTEM_INFO sysinf;
		GetNativeSystemInfo(&sysinf);
		pgsz = sysinf.dwPageSize;
	}
	if (curpg == NULL || (pgoff + sz) > pgsz)
	{
		curpg = (char*)VirtualAlloc(NULL, pgsz, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
		pgoff = 0;
	}

#pragma warning(suppress:6011)
	char* result = &curpg[pgoff];

	pgoff += sz;
	if (pgoff % 0x10 != 0)
		pgoff += 0x10 - (pgoff % 0x10);
	return result;
}

void* const CopyFunction(intptr_t address, int bytes)
{
	char* code = AllocateCode(bytes);
	memcpy(code, address, bytes);
	return (void*)code;
}

static const char NIBBLE_LOOKUP[16] = {
  0, 1, 1, 2, 1, 2, 2, 3, 1, 2, 2, 3, 2, 3, 3, 4 };

int parametersSize = 0;
int* parameters;

void expandParameters(int newsize)
{
	if (newsize <= parametersSize)
		return;

	if (parameters != NULL)
		free(parameters);
	parameters = (int*)malloc(newsize * sizeof(int));

	parametersSize = newsize;
}

#pragma region Hook

char findMissingRegisters(int* args, int argCount, int ret)
{
	char flags = 0;
	if (ret && ret <= rDH)
		flags |= 1 << (ret % 4);

	for (int i = 0; i < argCount; i++)
	{
		int arg = args[i];
		if (arg > rDH)
			continue;
		flags |= 1 << ((arg - 1) % 4);
	}
	return ~flags & 0xF;
}

typedef struct registerHookInfo
{
	registers reg;
	int pushbytes;
	int retbytes;
	int popbytes;
	char bytes[4];
} registerHookInfo;

const registerHookInfo hookInfo[] = {
	{ noret,  0, 0,	0,								},
	{ rEAX,	  1, 0,	1,	{ 0x50, 0x58 }				},
	{ rEBX,	  1, 2,	1,	{ 0x53, 0x89, 0xC3, 0x5B }	},
	{ rECX,	  1, 2,	1,	{ 0x51, 0x89, 0xC1, 0x59 }	},
	{ rEDX,	  1, 2,	1,	{ 0x52, 0x89, 0xC2, 0x5A }	},
	{ rAX,	  1, 0,	1,	{ 0x50, 0x58 }				},
	{ rBX,	  1, 2,	1,	{ 0x53, 0x89, 0xC3, 0x5B }	},
	{ rCX,	  1, 2,	1,	{ 0x51, 0x89, 0xC1, 0x59 }	},
	{ rDX,	  1, 2,	1,	{ 0x52, 0x89, 0xC2, 0x5A }	},
	{ rAL,	  1, 0,	1,	{ 0x50, 0x58 }				},
	{ rBL,	  1, 2,	1,	{ 0x53, 0x89, 0xC3, 0x5B }	},
	{ rCL,	  1, 2,	1,	{ 0x51, 0x89, 0xC1, 0x59 }	},
	{ rDL,	  1, 2,	1,	{ 0x52, 0x89, 0xC2, 0x5A }	},
	{ rAH,	  0, 0,	1,  { 0x58 }					},
	{ rBH,	  0, 0,	1,	{ 0x5B }					},
	{ rCH,	  0, 0,	1,	{ 0x59 }					},
	{ rDH,	  0, 0,	1,	{ 0x5A }					},
	{ rESI,	  1, 2,	1,	{ 0x56, 0x89, 0xC6, 0x5E }	},
	{ rEDI,	  1, 2,	1,	{ 0x57, 0x89, 0xC7, 0x5F }	},
	{ rEBP,	  1, 2,	1,	{ 0x55, 0x89, 0xC5, 0x5D }	},
	{ rSI,	  1, 2,	1,	{ 0x56, 0x89, 0xC6, 0x5E }	},
	{ rDI,	  1, 2,	1,	{ 0x57, 0x89, 0xC7, 0x5F }	},
	{ rBP,	  1, 2,	1,	{ 0x55, 0x89, 0xC5, 0x5D }	},
	{ stack1, 4, 0,	0, 	{ 0xFF, 0x74, 0x24, 0x00 }	},
	{ stack2, 4, 0,	0, 	{ 0xFF, 0x74, 0x24, 0x00 }	},
	{ stack4, 4, 0,	0, 	{ 0xFF, 0x74, 0x24, 0x00 }	},
	{ rst0,  0, 0,	0,								},
};

void const GenerateUsercallHook(void* func, registers ret, unsigned short stdcallSize, intptr_t address, bool preserveABCD, int argCount, ...)
{
	// calculate memory size
	int memsz = 6; // call + ret
	int callOffset = 0;
	char stackSize = 0;
	int stackCorrections = 0;
	bool wasStack = false;

	// getting the return register info
	const registerHookInfo* rInfo = &hookInfo[ret];
	memsz += rInfo->retbytes;

	// working through the parameters (pop order) to
	// figure out the stack offset and correction count
	int* args = &argCount + 1;

	// add the missing registers to the arguments
	if (preserveABCD)
	{
		char regs = findMissingRegisters(args, argCount, ret);
		char regCount = NIBBLE_LOOKUP[regs];
		if (regCount)
		{
			expandParameters(argCount + regCount);
			memcpy(parameters, args, argCount * sizeof(int));
			args = parameters;

			if (regs & 0x1 && ret != rEAX) args[argCount++] = rEAX;
			if (regs & 0x2 && ret != rEBX) args[argCount++] = rEBX;
			if (regs & 0x4 && ret != rECX) args[argCount++] = rECX;
			if (regs & 0x8 && ret != rEDX) args[argCount++] = rEDX;
		}
	}

	for (int i = 0; i < argCount; i++)
	{
		int reg = args[i];
		rInfo = &hookInfo[reg];

		memsz += rInfo->pushbytes;
		callOffset += rInfo->pushbytes;

		if (reg >= stack1 && reg <= stack4)
		{
			stackSize += 4;
			if (!wasStack)
				stackCorrections++;
			wasStack = true;
		}
		else if (reg == ret)
		{
			if (!wasStack)
				stackCorrections++;
			wasStack = true;
		}
		else
		{
			memsz += rInfo->popbytes;
			wasStack = false;
		}
	}

	// each correction takes 3 bytes
	if (wasStack || stdcallSize > 0)
		memsz += 2;
	if (wasStack)
		stackCorrections--;
	memsz += stackCorrections * 3;

	// Allocating the size for the instructions
	char* instructions = AllocateCode(memsz);

	// writing push instructions
	int instrOffset = 0;
	char stackOffset = 0;
	for (int i = argCount - 1; i >= 0; i--)
	{
		int reg = args[i];
		rInfo = &hookInfo[reg];

		if (rInfo->pushbytes == 1)
			instructions[instrOffset] = rInfo->bytes[0];
		else if (rInfo->pushbytes > 1)
			memcpy(&instructions[instrOffset], &rInfo->bytes, rInfo->pushbytes);
		instrOffset += rInfo->pushbytes;

		if (reg >= stack1 && reg <= stack4)
			instructions[instrOffset - 1] = stackOffset + stackSize;
		else
			stackOffset += 4;
	}

	// writing the call
	WriteCall(&instructions[instrOffset], func);
	instrOffset += 5;

	// write the return parameter instruction
	rInfo = &hookInfo[ret];
	if (rInfo->retbytes > 0)
	{
		memcpy(&instructions[instrOffset], &rInfo->bytes[rInfo->pushbytes], rInfo->retbytes);
		instrOffset += rInfo->retbytes;
	}

	// writing the pop instructions
	wasStack = false;
	for (int i = 0; i < argCount; i++)
	{
		int reg = args[i];
		rInfo = &hookInfo[reg];

		if (reg == ret || (reg >= stack1 && reg <= stack4))
		{
			if (stackCorrections == 0)
			{
				stdcallSize += 4;
				continue;
			}
			if (wasStack)
			{
				// if the last operation already was a stack correction,
				// then just make it correct 4 more bytes
				instructions[instrOffset - 1] += 4;
			}
			else
			{
				instructions[instrOffset++] = 0x83;
				instructions[instrOffset++] = 0xC4;
				instructions[instrOffset++] = 4;
			}
			wasStack = true;
		}
		else
		{
			if (wasStack)
				stackCorrections--;

			if (rInfo->popbytes == 1)
				instructions[instrOffset] = rInfo->bytes[rInfo->pushbytes + rInfo->retbytes];
			else if (rInfo->pushbytes > 1)
				memcpy(&instructions[instrOffset], &rInfo->bytes[rInfo->pushbytes + rInfo->retbytes], rInfo->popbytes);
			instrOffset += rInfo->popbytes;
			wasStack = false;
		}
	}

	// writing the return
	if (stdcallSize > 0)
	{
		instructions[instrOffset++] = 0xC2;
		*(short*)&instructions[instrOffset] = stdcallSize;
		instrOffset += 2;
	}
	else
		instructions[instrOffset++] = 0xC3;

	assert(instrOffset == memsz);
	WriteJump((void*)address, instructions);
}

#pragma endregion

#pragma region Wrapper

typedef struct registerWrapperInfo
{
	registers reg;
	int movbytes;
	int retbytes;
	char bytes[10];
} registerWrapperInfo;

const registerWrapperInfo wrapperInfo[] = {
	{ noret,  0, 0,	},
	{ rEAX,   4, 0, { 0x8B, 0x44, 0x24, 0x00 } },
	{ rEBX,   4, 2, { 0x8B, 0x5C, 0x24, 0x00, 0x89, 0xD8 } },
	{ rECX,   4, 2, { 0x8B, 0x4C, 0x24, 0x00, 0x89, 0xC8 } },
	{ rEDX,   4, 2, { 0x8B, 0x54, 0x24, 0x00, 0x89, 0xD0 } },
	{ rAX,    5, 3, { 0x66, 0x8B, 0x44, 0x24, 0x00, 0x0F, 0xBF, 0xC0 } },
	{ rBX,    5, 3, { 0x66, 0x8B, 0x5C, 0x24, 0x00, 0x0F, 0xBF, 0xC3 } },
	{ rCX,    5, 3, { 0x66, 0x8B, 0x4C, 0x24, 0x00, 0x0F, 0xBF, 0xC1 } },
	{ rDX,    5, 3, { 0x66, 0x8B, 0x54, 0x24, 0x00, 0x0F, 0xBF, 0xC2 } },
	{ rAL,    4, 3, { 0x8A, 0x44, 0x24, 0x00, 0x0F, 0xBE, 0xC0 } },
	{ rBL,    4, 3, { 0x8A, 0x5C, 0x24, 0x00, 0x0F, 0xBE, 0xC3 } },
	{ rCL,    4, 3, { 0x8A, 0x4C, 0x24, 0x00, 0x0F, 0xBE, 0xC1 } },
	{ rDL,    4, 3, { 0x8A, 0x54, 0x24, 0x00, 0x0F, 0xBE, 0xC2 } },
	{ rAH,    4, 3, { 0x8A, 0x64, 0x24, 0x00, 0x0F, 0xBE, 0xC4 } },
	{ rBH,    4, 3, { 0x8A, 0x7C, 0x24, 0x00, 0x0F, 0xBE, 0xC7 } },
	{ rCH,    4, 3, { 0x8A, 0x6C, 0x24, 0x00, 0x0F, 0xBE, 0xC5 } },
	{ rDH,    4, 3, { 0x8A, 0x74, 0x24, 0x00, 0x0F, 0xBE, 0xC6 } },
	{ rESI,   4, 2, { 0x8B, 0x74, 0x24, 0x00, 0x89, 0xF0 } },
	{ rEDI,   4, 2, { 0x8B, 0x7C, 0x24, 0x00, 0x89, 0xF8 } },
	{ rEBP,   4, 2, { 0x8B, 0x6C, 0x24, 0x00, 0x89, 0xE8 } },
	{ rSI,    4, 3, { 0x66, 0x8B, 0x74, 0x24, 0x00, 0x0F, 0xBF, 0xC6 } },
	{ rDI,    4, 3, { 0x66, 0x8B, 0x7C, 0x24, 0x00, 0x0F, 0xBF, 0xC7 } },
	{ rBP,    4, 3, { 0x66, 0x8B, 0x6C, 0x24, 0x00, 0x0F, 0xBF, 0xC5 } },
	{ stack1, 6, 0, { 0x0F, 0xBE, 0x44, 0x24, 0x00, 0x50} },
	{ stack2, 6, 0, { 0x0F, 0xBF, 0x44, 0x24, 0x00, 0x50} },
	{ stack4, 4, 0, { 0xFF, 0x74, 0x24, 0x00 } },
	{ rst0,   0, 8, { 0xD9, 0x5C, 0x24, 0xFC, 0x8B, 0x44, 0x24, 0xFC } }
};

void* const GenerateUsercallWrapper(registers ret, unsigned short stdcallSize, intptr_t address, bool preserveABCD, int argCount, ...)
{
	// calculate memory size
	int memsz = 6; // call + ret
	char usedRegs = 0;
	char stacksize = 0;

	if(preserveABCD)
		usedRegs = 0xF;

	// determining memory size
	int* args = &argCount + 1;
	for (int i = 0; i < argCount; i++)
	{
		int reg = args[i];
		if(reg == noret)
			continue;
		
		const registerWrapperInfo* wInfo = &wrapperInfo[reg];
		memsz += wInfo->movbytes;
		
		if(reg <= rDH)
			usedRegs |= 1 << ((reg - rEAX) % 4);
		else if(reg <= rBP)
			usedRegs |= 0x10 << ((reg - rESI) % 3);
		else if(reg >= stack1 && reg <= stack4)
			stacksize += 4;
	}
	
	// we dont have to preserve eax if something is returned
	if(ret != noret)
		usedRegs &= 0xFE;

	// each preservation takes up 2 bytes: push and pop
	memsz += ( NIBBLE_LOOKUP[usedRegs & 0xF] 
			 + NIBBLE_LOOKUP[(usedRegs >> 4) & 0xF]
			 ) * 2;
	
	stacksize -= stdcallSize;

	// either retn bytes or add esp, size
	if(stacksize > 0)
		memsz += usedRegs == 0 ? 2 : 3;

	// allocating the memory
	char* instructions = AllocateCode(memsz);

	// writing the pushes
	int instrOffset = 0;
	char stackoffset = 4;
	if(argCount > 0)
		stackoffset += 4 * (argCount - 1);

	for (int i = 0; i < 7; i++)
	{
		if(!((usedRegs >> i) & 0x1))
			continue;
		
		const registerHookInfo* hInfo;
		if(i <= 3)
			hInfo = &hookInfo[i + rEAX];
		else
			hInfo = &hookInfo[i - 4 + rESI];
			
		instructions[instrOffset++] = hInfo->bytes[0];
		stackoffset += 4;
	}
	stacksize = 0;
	// writing the movs
	for (int i = argCount - 1; i >= 0; i--)
	{
		int reg = args[i];
		if (reg == noret)
			continue;

		const registerWrapperInfo* wInfo = &wrapperInfo[reg];
		memcpy(&instructions[instrOffset], wInfo->bytes, wInfo->movbytes);
		instrOffset += wInfo->movbytes;

		if(reg != stack1 && reg != stack2)
			instructions[instrOffset - 1] = stackoffset;
		else
			instructions[instrOffset - 2] = stackoffset;

		if(reg >= stack1 && reg <= stack4)
			stacksize += 4;
		else
			stackoffset -= 4;
	}

	// writing the call
	WriteCall(&instructions[instrOffset], (void*)address);
	instrOffset += 5;
	stacksize -= stdcallSize;

	// writing the return variable
	if (ret > rEAX)
	{
		#pragma warning(suppress:33011)
		const registerWrapperInfo* wInfo = &wrapperInfo[ret];
		memcpy(&instructions[instrOffset], wInfo->bytes[wInfo->movbytes], wInfo->retbytes);
		instrOffset += wInfo->retbytes;
	}

	if (stacksize > 0)
	{
		if (usedRegs == 0)
		{
			// return with stack adjustment
			instructions[instrOffset++] = 0xC2;
			*(short*)&instructions[instrOffset] = stacksize;
			instrOffset += 2;
		}
		else
		{
			// add esp, stacksize
			instructions[instrOffset++] = 0x83;
			instructions[instrOffset++] = 0xC4;
			instructions[instrOffset++] = stacksize;
		}
	}

	if (usedRegs > 0)
	{
		// writing the pops
		for (int i = 7; i >= 0; i--)
		{
			if (!((usedRegs >> i) & 0x1))
				continue;

			registerHookInfo* hInfo;
			if (i <= 3)
				hInfo = &hookInfo[i + rEAX];
			else
				hInfo = &hookInfo[i - 4 + rESI];

			
			instructions[instrOffset++] = hInfo->bytes[hInfo->pushbytes + hInfo->retbytes];
		}
		
		// return
		instructions[instrOffset++] = 0xC3;
	}

	assert(instrOffset == memsz);
	return (void*)instructions;
}

#pragma endregion
#endif