#pragma once

enum class flipmode
{
	flipmode_None,
	flipmode_Horizontal,
	flipmode_Vertical
};

#define PI 3.14159265358979323846f
const float Rad2Deg = 180.0f / PI;

extern flipmode active_flipmode;
extern float rotationSpeed;
extern float rotationRadians;

void hookFlipScreen();