#include "flipscreen.h"
#include <math.h>
#include "MemAccess.h"

typedef struct Vector3
{
	float x;
	float y;
	float z;
} Vector3;

typedef struct Matrix4x4
{
	float m00; float m01; float m02; float m03;
	float m10; float m11; float m12; float m13;
	float m20; float m21; float m22; float m23;
	float m30; float m31; float m32; float m33;
} Matrix4x4;

const void* vector3_normalize_loc = (void*)0x00427CC0;

void __declspec(naked) vector3_Normalize(Vector3* input, Vector3* output)
{
	__asm
	{
		push        ebx
		push        ecx
		push        edx
		push        esi
		push        edi

		mov         esi, dword ptr[esp + 1Ch]
		mov         edi, dword ptr[esp + 18h]
		call        vector3_normalize_loc

		pop         edi
		pop         esi
		pop         edx
		pop         ecx
		pop         ebx

		ret
	}
}

flipmode active_flipmode = flipmode::flipmode_None;
float rotationSpeed = 0;
float rotationRadians = 0;

void rotateVector(Vector3* axis, Vector3* target)
{
	float sine = sinf(rotationRadians);
	float cosine = cosf(rotationRadians);
	float oneMinusCosine = 1.0f - cosine;
	oneMinusCosine *= axis->x * target->x + axis->y * target->y + axis->z * target->z;

#define formula(c, c1, c2) \
		target->##c * cosine \
		+ (target->##c1 * axis->##c2 - target->##c2 * axis->##c1) * sine \
		+ axis->##c * oneMinusCosine

	Vector3 result = {
		formula(x, y, z),
		formula(y, z, x),
		formula(z, x, y),
	};

	vector3_Normalize(&result, target);
}

Matrix4x4* matrix4x4_Lookat(Vector3* origin, Vector3* target, Vector3* up, Matrix4x4* output)
{
	Vector3 lookdir = {
		target->x - origin->x,
		target->y - origin->y,
		target->z - origin->z
	};

	Vector3 lookdirN;
	vector3_Normalize(&lookdir, &lookdirN);

	rotationRadians += rotationSpeed / Rad2Deg;
	if (rotationRadians != 0.0f)
	{
		rotateVector(&lookdirN, up);
	}

	Vector3 tmp = {
		up->y * lookdirN.z - up->z * lookdirN.y,
		up->z * lookdirN.x - up->x * lookdirN.z,
		up->x * lookdirN.y - up->y * lookdirN.x
	};

	Vector3 tmpN;
	vector3_Normalize(&tmp, &tmpN);

	Vector3 tmp2 = {
		lookdirN.y * tmpN.z - lookdirN.z * tmpN.y,
		lookdirN.z * tmpN.x - lookdirN.x * tmpN.z,
		lookdirN.x * tmpN.y - lookdirN.y * tmpN.x
	};

	if (active_flipmode == flipmode::flipmode_Vertical)
	{
		output->m00 = -tmpN.x;
		output->m10 = -tmpN.y;
		output->m20 = -tmpN.z;
		output->m30 = tmpN.x * target->x + tmpN.y * target->y + tmpN.z * target->z;
	}
	else
	{
		output->m00 = tmpN.x;
		output->m10 = tmpN.y;
		output->m20 = tmpN.z;
		output->m30 = -(tmpN.x * target->x + tmpN.y * target->y + tmpN.z * target->z);
	}

	if (active_flipmode == flipmode::flipmode_Horizontal)
	{
		output->m01 = -tmp2.x;
		output->m11 = -tmp2.y;
		output->m21 = -tmp2.z;
		output->m31 = tmp2.x * target->x + tmp2.y * target->y + tmp2.z * target->z;
	}
	else
	{
		output->m01 = tmp2.x;
		output->m11 = tmp2.y;
		output->m21 = tmp2.z;
		output->m31 = -(tmp2.x * target->x + tmp2.y * target->y + tmp2.z * target->z);
	}

	output->m02 = lookdirN.x;
	output->m12 = lookdirN.y;
	output->m22 = lookdirN.z;
	output->m32 = -(lookdirN.x * target->x + lookdirN.y * target->y + lookdirN.z * target->z);

	output->m03 = 0.0f;
	output->m13 = 0.0f;
	output->m23 = 0.0f;
	output->m33 = 1.0f;

	return output;
}

void __declspec(naked) matrix4x4_Lookat_hook()
{
	__asm
	{
		push        edx
		push        ecx
		push        ebx
		push        dword ptr[esp + 14h]
		push        dword ptr[esp + 14h]
		push        eax
		call        matrix4x4_Lookat
		add         esp, 0Ch
		pop         ebx
		pop         ecx
		pop         edx
		ret
	}
}

void hookFlipScreen()
{
	WriteJump((void*)0x00427AA0, matrix4x4_Lookat_hook);
}