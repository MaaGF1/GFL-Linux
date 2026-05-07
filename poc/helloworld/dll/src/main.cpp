/**
 * @file poc/helloworld/dll/src/main.cpp
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

typedef const char* (*GetHelloWorldStringFunc)();
typedef int (*CalculateAddFunc)(int, int);

int main()
{
	printf("[*] Windows Native Loader Started.\n");
	
	HMODULE hModule = LoadLibraryA("helloworld.dll");
	if (!hModule)
	{
		printf("[-] Failed to load helloworld.dll\n");
		return 1;
	}

	GetHelloWorldStringFunc getStr = (GetHelloWorldStringFunc)GetProcAddress(hModule, "GetHelloWorldString");
	CalculateAddFunc calcAdd = (CalculateAddFunc)GetProcAddress(hModule, "CalculateAdd");

	if (getStr && calcAdd)
	{
		printf("[+] DLL Loaded successfully.\n");
		printf("[+] GetHelloWorldString: %s\n", getStr());
		printf("[+] CalculateAdd(10, 20): %d\n", calcAdd(10, 20));
	}
	else
	{
		printf("[-] Failed to find exported functions.\n");
	}

	FreeLibrary(hModule);
	return 0;
}