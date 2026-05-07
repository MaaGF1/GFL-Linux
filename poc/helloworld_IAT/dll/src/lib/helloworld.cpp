/**
 * @file poc/helloworld_IAT/dll/src/lib/helloworld.cpp
 * @author @SwordofMorning
 * @brief Dynamic library functions with IAT.
 * @version 0.1
 * @date 2026-05-07
 * 
 * @copyright Copyright (c) 2026
 * 
 */

#include <windows.h>

#define EXPORT_API extern "C" __declspec(dllexport)

EXPORT_API void TestIATCalls()
{
	// Test 1: Single argument Win32 API (KERNEL32.dll)
	OutputDebugStringA("[+] OutputDebugStringA called from Windows DLL!");

	// Test 2: 4-argument Win32 API (USER32.dll)
	MessageBoxA(NULL, "This is MessageBox text from Windows Memory", "Win32 Dialog", MB_OK);
}