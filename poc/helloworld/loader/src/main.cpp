/**
 * @file poc/helloworld/loader/src/main.cpp
 * @author @SwordofMorning
 * @brief DLL loader on linux.
 * @version 0.1
 * @date 2026-05-07
 * 
 * @copyright Copyright (c) 2026
 * 
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>

// =========================================================================
// Exact PE64 Structures (1-byte packing)
// =========================================================================

typedef uint8_t  BYTE;
typedef uint16_t WORD;
typedef uint32_t DWORD;
typedef uint64_t QWORD;

#pragma pack(push, 1)

typedef struct _IMAGE_DOS_HEADER
{
	WORD e_magic;
	BYTE reserved[58];
	DWORD e_lfanew;
} IMAGE_DOS_HEADER;

typedef struct _IMAGE_FILE_HEADER
{
	WORD Machine;
	WORD NumberOfSections;
	DWORD TimeDateStamp;
	DWORD PointerToSymbolTable;
	DWORD NumberOfSymbols;
	WORD SizeOfOptionalHeader;
	WORD Characteristics;
} IMAGE_FILE_HEADER;

typedef struct _IMAGE_DATA_DIRECTORY
{
	DWORD VirtualAddress;
	DWORD Size;
} IMAGE_DATA_DIRECTORY;

typedef struct _IMAGE_OPTIONAL_HEADER64
{
	WORD Magic;
	BYTE MajorLinkerVersion;
	BYTE MinorLinkerVersion;
	DWORD SizeOfCode;
	DWORD SizeOfInitializedData;
	DWORD SizeOfUninitializedData;
	DWORD AddressOfEntryPoint;
	DWORD BaseOfCode;
	QWORD ImageBase;
	DWORD SectionAlignment;
	DWORD FileAlignment;
	WORD MajorOperatingSystemVersion;
	WORD MinorOperatingSystemVersion;
	WORD MajorImageVersion;
	WORD MinorImageVersion;
	WORD MajorSubsystemVersion;
	WORD MinorSubsystemVersion;
	DWORD Win32VersionValue;
	DWORD SizeOfImage;
	DWORD SizeOfHeaders;
	DWORD CheckSum;
	WORD Subsystem;
	WORD DllCharacteristics;
	QWORD SizeOfStackReserve;
	QWORD SizeOfStackCommit;
	QWORD SizeOfHeapReserve;
	QWORD SizeOfHeapCommit;
	DWORD LoaderFlags;
	DWORD NumberOfRvaAndSizes;
	IMAGE_DATA_DIRECTORY DataDirectory[16];
} IMAGE_OPTIONAL_HEADER64;

typedef struct _IMAGE_NT_HEADERS64
{
	DWORD Signature;
	IMAGE_FILE_HEADER FileHeader;
	IMAGE_OPTIONAL_HEADER64 OptionalHeader;
} IMAGE_NT_HEADERS64;

typedef struct _IMAGE_SECTION_HEADER
{
	BYTE Name[8];
	DWORD VirtualSize;
	DWORD VirtualAddress;
	DWORD SizeOfRawData;
	DWORD PointerToRawData;
	DWORD PointerToRelocations;
	DWORD PointerToLinenumbers;
	WORD NumberOfRelocations;
	WORD NumberOfLinenumbers;
	DWORD Characteristics;
} IMAGE_SECTION_HEADER; // Exactly 40 bytes

typedef struct _IMAGE_EXPORT_DIRECTORY
{
	DWORD Characteristics;
	DWORD TimeDateStamp;
	WORD MajorVersion;
	WORD MinorVersion;
	DWORD Name;
	DWORD Base;
	DWORD NumberOfFunctions;
	DWORD NumberOfNames;
	DWORD AddressOfFunctions;
	DWORD AddressOfNames;
	DWORD AddressOfNameOrdinals;
} IMAGE_EXPORT_DIRECTORY;

#pragma pack(pop)

// =========================================================================
// Win64 to SysV ABI Thunk
// =========================================================================

// Thunk for: int function(int a, int b)
// Linux passes: RDI (a), RSI (b)
// Windows expects: RCX (a), RDX (b)
extern "C" int CallWin64Func_2Args(void* func_ptr, int arg1, int arg2)
{
	int result = 0;
	__asm__ volatile (
		"sub $32, %%rsp \n"						// [Win64 ABI] Allocate 32 bytes of shadow space
		"call *%1 \n"							// Call the Windows function pointer
		"add $32, %%rsp \n"						// Clean up the shadow space
		: "=a" (result)							// Output: Return value in EAX mapped to 'result'
		: "r" (func_ptr),						// Input 1 (%1): func_ptr in any general register
		  "c" (arg1),							// Input 2: Force arg1 into ECX/RCX (Win64 Arg 1)
		  "d" (arg2)							// Input 3: Force arg2 into EDX/RDX (Win64 Arg 2)
		: "r8", "r9", "r10", "r11", "memory"	// Clobbered registers (caller-saved)
	);
	return result;
}

// =========================================================================
// PE Loader Core
// =========================================================================

int main(int argc, char** argv)
{
	printf("[*] Linux Custom PE Loader Started.\n");

	if (argc < 2)
	{
		printf("Usage: %s <path_to_dll>\n", argv[0]);
		return 1;
	}

	// 1. Read raw DLL file into memory
	int fd = open(argv[1], O_RDONLY);
	if (fd < 0)
	{
		perror("[-] Failed to open DLL");
		return 1;
	}

	struct stat st;
	fstat(fd, &st);
	BYTE* raw_data = (BYTE*)malloc(st.st_size);
	if (read(fd, raw_data, st.st_size) < 0)
	{
		close(fd);
		perror("[-] Failed to read fd");
		return 1;
	}
	close(fd);

	// 2. Parse PE Headers
	IMAGE_DOS_HEADER* dos_header = (IMAGE_DOS_HEADER*)raw_data;
	IMAGE_NT_HEADERS64* nt_headers = (IMAGE_NT_HEADERS64*)(raw_data + dos_header->e_lfanew);

	// 3. Allocate executable memory for the mapped image
	DWORD image_size = nt_headers->OptionalHeader.SizeOfImage;
	BYTE* mapped_base = (BYTE*)mmap(NULL, image_size, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	
	printf("[+] Allocated %u bytes of executable memory at %p\n", image_size, mapped_base);

	// 4. Map PE Headers
	memcpy(mapped_base, raw_data, nt_headers->OptionalHeader.SizeOfHeaders);
	
	// Safely calculate Section Header starting point
	IMAGE_SECTION_HEADER* section = (IMAGE_SECTION_HEADER*)(
		(BYTE*)nt_headers + 
		4 + // Signature
		sizeof(IMAGE_FILE_HEADER) + 
		nt_headers->FileHeader.SizeOfOptionalHeader
	);
	
	printf("[*] Mapping %d Sections...\n", nt_headers->FileHeader.NumberOfSections);
	for (int i = 0; i < nt_headers->FileHeader.NumberOfSections; i++)
	{
		// Create a null-terminated string for safe printing
		char sec_name[9] = {0};
		memcpy(sec_name, section[i].Name, 8);
		
		if (section[i].SizeOfRawData > 0)
		{
			memcpy(mapped_base + section[i].VirtualAddress, 
				   raw_data + section[i].PointerToRawData, 
				   section[i].SizeOfRawData);
			printf("	-> Mapped Section %-8s | RVA: 0x%04X | Size: %d\n", 
				   sec_name, section[i].VirtualAddress, section[i].SizeOfRawData);
		} else
		{
			printf("	-> Skipped Section %-8s (No Raw Data)\n", sec_name);
		}
	}

	// 5. Parse Export Directory to find function addresses
	DWORD export_rva = nt_headers->OptionalHeader.DataDirectory[0].VirtualAddress;
	if (export_rva == 0)
	{
		printf("[-] No Export Directory found.\n");
		return 1;
	}

	IMAGE_EXPORT_DIRECTORY* export_dir = (IMAGE_EXPORT_DIRECTORY*)(mapped_base + export_rva);
	DWORD* name_rvas = (DWORD*)(mapped_base + export_dir->AddressOfNames);
	WORD* ordinal_rvas = (WORD*)(mapped_base + export_dir->AddressOfNameOrdinals);
	DWORD* function_rvas = (DWORD*)(mapped_base + export_dir->AddressOfFunctions);

	void* func_GetHelloWorldString = NULL;
	void* func_CalculateAdd = NULL;

	printf("[*] Scanning Export Table (Found %d names)...\n", export_dir->NumberOfNames);
	for (DWORD i = 0; i < export_dir->NumberOfNames; i++)
	{
		char* func_name = (char*)(mapped_base + name_rvas[i]);
		WORD ordinal = ordinal_rvas[i];
		DWORD func_rva = function_rvas[ordinal];
		void* func_addr = mapped_base + func_rva;

		// Print everything it finds to debug
		printf("	-> Export [%d]: %s at %p\n", i, func_name, func_addr);

		if (strcmp(func_name, "GetHelloWorldString") == 0)
		{
			func_GetHelloWorldString = func_addr;
		}
		if (strcmp(func_name, "CalculateAdd") == 0)
		{
			func_CalculateAdd = func_addr;
		}
	}

	if (func_GetHelloWorldString)
	{
		typedef const char* (*GetStrFunc)();
		GetStrFunc get_str = (GetStrFunc)func_GetHelloWorldString;
		printf("\n[EXEC] Calling GetHelloWorldString...\n");
		printf("[OUT]  %s\n", get_str());
	} else
	{
		printf("\n[-] Could not find GetHelloWorldString\n");
	}

	if (func_CalculateAdd)
	{
		// Two arguments. Must use ABI Thunk to convert registers.
		printf("\n[EXEC] Calling CalculateAdd(15, 25) via ABI Thunk...\n");
		int res = CallWin64Func_2Args(func_CalculateAdd, 15, 25);
		printf("[OUT]  Result: %d\n", res);
	}

	// Cleanup
	munmap(mapped_base, image_size);
	free(raw_data);
	printf("\n[*] Done.\n");
	return 0;
}