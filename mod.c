#include "SA2ModInfo.h"
#include "flipscreen.h"
#include "IniFile.hpp"

extern "C"
{

	__declspec(dllexport) void Init(const char* path, const HelperFunctions* helperFunctions)
	{
		const IniFile* settings = new IniFile(std::string(path) + "\\config.ini");

		std::string sFlipmode = settings->getString("Settings", "Flipmode");
		if (sFlipmode._Equal("Horizontal"))
		{
			active_flipmode = flipmode::flipmode_Horizontal;
		}
		else if (sFlipmode._Equal("Vertical"))
		{
			active_flipmode = flipmode::flipmode_Vertical;
		}

		rotationRadians = settings->getFloat("Settings", "Rotate Screen") / Rad2Deg;
		rotationSpeed = settings->getFloat("Settings", "Rotation Animation Speed") / Rad2Deg;

		if (rotationRadians != 0.0f || rotationSpeed != 0.0f || active_flipmode != flipmode::flipmode_None)
		{
			hookFlipScreen();
		}
	}

	__declspec(dllexport) ModInfo SA2ModInfo = { ModLoaderVer };
}