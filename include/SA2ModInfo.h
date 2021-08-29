#pragma once

#ifdef COMPILE_MOD
#include <Windows.h>

/**
 * SA2 Mod Loader.
 * Mod metadata structures.
 */

#define ModLoaderVer 8

typedef struct PatchInfo
{
	void* address;
	const void* data;
	int datasize;
} PatchInfo;

typedef struct PatchList
{
	const PatchInfo* Patches;
	int Count;
} PatchList;

typedef struct PointerInfo
{
	void* address;
	void* data;
} PointerInfo;

typedef struct PointerList
{
	const PointerInfo* Pointers;
	int Count;
} PointerList;

#define patchdecl(address,data) { (void*)address, arrayptrandsize(data) }
#define ptrdecl(address,data) { (void*)address, (void*)data }

#undef ReplaceFile // WinAPI function
typedef struct HelperFunctions
{
	// The version of the structure.
	int Version;
	// startposition functions (those are not needed for the decomp either way)
	int startPosFunctions[4];
	// Returns the path where main game save files are stored.
	// Requires version >= 4.
	const char* (__cdecl* GetMainSavePath)();
	// Returns the path where Chao save files are stored.
	// Requires version >= 4.
	const char* (__cdecl* GetChaoSavePath)();
	// startposition functions (those are not needed for the decomp either way)
	int endPosfunctions[4];
	// Replaces data exported from the Data DLL with your own data.
	// Requires version >= 6.
	void(__cdecl* HookExport)(LPCSTR exportName, const void* newdata);
	/**
	* @brief Gets the real path to a replaceable file.
	*
	* If your mod contains files in its SYSTEM folder that it loads manually,
	* you can use this function to retrieve the full path to the file. This
	* allows other mods to replace this file without any extra work from you.
	* Requires version >= 7.
	*
	* @param path The file path (e.g "resource\\gd_PC\\my_cool_file.bin")
	* @return The replaced path to the file.
	*/
	const char* (__cdecl* GetReplaceablePath)(const char* path);
	// Replaces the source file with the destination file.
	// Requires version >= 7.
	void(__cdecl* ReplaceFile)(const char* src, const char* dst);
	// Sets the window title.
	// Requires version >= 7.
	void(__cdecl* SetWindowTitle)(const wchar_t* title);
	// Sets the size of the debug font, defaults to 12.
	// Requires version >= 8
	void(__cdecl* SetDebugFontSize)(float size);
	// Sets the argb color of the debug font, defaults to 0xFFBFBFBF.
	// Requires version >= 8
	void(__cdecl* SetDebugFontColor)(int color);
	// Displays a string on screen at a specific location (using NJM_LOCATION)
	// Example: DisplayDebugString(NJM_LOCATION(x, y), "string");
	// Requires version >= 8
	void(__cdecl* DisplayDebugString)(int loc, const char* str);
	// Displays a formatted string on screen at a specific location (using NJM_LOCATION)
	// Requires version >= 8
	void(__cdecl* DisplayDebugStringFormatted)(int loc, const char* Format, ...);
	// Displays a number on screen at a specific location (using NJM_LOCATION)
	// If the number of digits is superior, it will add leading zeroes.
	// Example: DisplayDebugNumber(NJM_LOCATION(x, y), 123, 5); will display 00123.
	// Requires version >= 8
	void(__cdecl* DisplayDebugNumber)(int loc, int value, int numdigits);
} HelperFunctions;

typedef void(__cdecl* ModInitFunc)(const char* path, const HelperFunctions* helperFunctions);

typedef void(__cdecl* ModEvent)();

typedef struct ModInfo
{
	int Version;
	void(__cdecl* Init)(const char* path, const HelperFunctions* helperFunctions);
	const PatchInfo* Patches;
	int PatchCount;
	const PointerInfo* Jumps;
	int JumpCount;
	const PointerInfo* Calls;
	int CallCount;
	const PointerInfo* Pointers;
	int PointerCount;
} ModInfo;

#endif