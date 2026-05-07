/**
 * @file poc/helloworld_IAT/dll/src/main.cpp
 * @author @SwordofMorning
 * @brief DLL loader on Windows
 * @version 0.1
 * @date 2026-05-07
 * 
 * @copyright Copyright (c) 2026
 * 
 */

#include <windows.h>
#include <stdio.h>

typedef void (*TestIATCallsFunc)();

int main()
{
	printf("[*] Windows Native Loader (IAT Test) Started.\n");

	HMODULE hModule = LoadLibraryA("helloworld.dll");
	if (!hModule)
	{
		printf("[-] Failed to load helloworld.dll\n");
		return 1;
	}

	TestIATCallsFunc testCalls = (TestIATCallsFunc)GetProcAddress(hModule, "TestIATCalls");

	if (testCalls)
	{
		printf("[+] DLL Loaded successfully. Invoking TestIATCalls...\n");
		testCalls();
		printf("[+] TestIATCalls completed.\n");
	}
	else
	{
		printf("[-] Failed to find exported function.\n");
	}

	FreeLibrary(hModule);
	return 0;
}