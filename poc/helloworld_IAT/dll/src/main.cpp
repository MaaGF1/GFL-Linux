/**
 * @file poc/helloworld_IAT/dll/src/main.cpp
 * @brief DLL loader for local Windows test.
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