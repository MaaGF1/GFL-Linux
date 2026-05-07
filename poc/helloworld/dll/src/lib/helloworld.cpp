/**
 * @file poc/helloworld/dll/src/lib/helloworld.cpp
 * @author @SwordofMorning
 * @brief Dynamic library functions.
 * @version 0.1
 * @date 2026-05-07
 * 
 * @copyright Copyright (c) 2026
 * 
 */

#define EXPORT_API extern "C" __declspec(dllexport)

// Test 1: Return a static string pointer (Tests .rdata section mapping)
EXPORT_API const char* GetHelloWorldString()
{
	return "[+] Hello World from Windows DLL running on Linux memory!";
}

// Test 2: Perform calculation (Tests .text execution and ABI parameter passing)
EXPORT_API int CalculateAdd(int a, int b)
{
	return a + b + 100; // Add 100 to prove it's actually executing this logic
}