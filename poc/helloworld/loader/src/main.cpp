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
	WORD e_magic;				// File First Byte: "MZ" (0x5A4D i.e. littleend 0x4D5A)
	BYTE reserved[58];			// Historical waste
	DWORD e_lfanew;				// Point to real PE header (Long File Address New), begin at 0x3C (Decimal 59)
} IMAGE_DOS_HEADER;

typedef struct _IMAGE_FILE_HEADER
{
	WORD Machine;
	WORD NumberOfSections;			// Code block, data block etc.
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
	DWORD SizeOfImage;								// How many memory need to mmap in Linux
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
	IMAGE_DATA_DIRECTORY DataDirectory[16];			// Where is Export Table in Portable Executable
} IMAGE_OPTIONAL_HEADER64;

typedef struct _IMAGE_NT_HEADERS64
{
	DWORD Signature;								// Sign: "PE\0\0" (0x00004550 i.e. littleend 0x50450000)
	IMAGE_FILE_HEADER FileHeader;					// Physical file information
	IMAGE_OPTIONAL_HEADER64 OptionalHeader;			// Memory loading information
} IMAGE_NT_HEADERS64;

/**
 * @brief DLL Section
 * 
 * @note in .dll file, code have been store as:
 * 1. .text: machine code (assembly instructions)
 * 2. .rdata: constant variable
 * 3. .data: global variable
 * 
 */
typedef struct _IMAGE_SECTION_HEADER
{
	BYTE Name[8];
	DWORD VirtualSize;
	DWORD VirtualAddress;							// Where in memory (mapped_base) should this segment be moved
	DWORD SizeOfRawData;
	DWORD PointerToRawData;							// The location of this segment in the hard drive file (raw_data)
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
// Linux: RDI, RSI, RDX, RCX, R8, R9
// Windows: RCX(a), RDX(b), R8, R9
extern "C" int CallWin64Func_2Args(void* func_ptr, int arg1, int arg2)
{
	int result = 0;
	__asm__ volatile (
		"sub $32, %%rsp \n"						// [Win64 ABI] Allocate 32 bytes of shadow space
		"call *%1 \n"							// Call the Windows function pointer
		"add $32, %%rsp \n"						// Clean up the shadow space
		: "=a" (result)							// Output: Return value in EAX/RAX mapped to 'result'
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

	// ----- 1. Read raw DLL file into memory -----

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

	// ----- 2. Parse PE Headers -----

	// Read raw file
	IMAGE_DOS_HEADER* dos_header = (IMAGE_DOS_HEADER*)raw_data;
	// Offset the pointer to NT
	IMAGE_NT_HEADERS64* nt_headers = (IMAGE_NT_HEADERS64*)(raw_data + dos_header->e_lfanew);

	// ----- 3. Allocate executable memory for the mapped image -----

	// To match the CPU's page alignment (typically 4KB), it will be stretched, thus we use `SizeOfImage`
	DWORD image_size = nt_headers->OptionalHeader.SizeOfImage;
	// Allocate memory with `PROT_EXEC`
	BYTE* mapped_base = (BYTE*)mmap(NULL, image_size, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

	printf("[+] Allocated %u bytes of executable memory at %p\n", image_size, mapped_base);

	// ----- 4. Map PE Headers -----

	// Copy NT image to mapped_base
	memcpy(mapped_base, raw_data, nt_headers->OptionalHeader.SizeOfHeaders);

	// Safely calculate Section Header starting point
	IMAGE_SECTION_HEADER* section = (IMAGE_SECTION_HEADER*)(
		(BYTE*)nt_headers + 
		4 + // Signature
		sizeof(IMAGE_FILE_HEADER) + 
		nt_headers->FileHeader.SizeOfOptionalHeader
	);
	
	// Traverse all sections
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

	// ----- 5. Parse Export Directory to find function addresses -----

	DWORD export_rva = nt_headers->OptionalHeader.DataDirectory[0].VirtualAddress;
	if (export_rva == 0)
	{
		printf("[-] No Export Directory found.\n");
		return 1;
	}

	IMAGE_EXPORT_DIRECTORY* export_dir = (IMAGE_EXPORT_DIRECTORY*)(mapped_base + export_rva);
	// Pointer to function name
	DWORD* name_rvas = (DWORD*)(mapped_base + export_dir->AddressOfNames);
	// The index number corresponding to the function name
	WORD* ordinal_rvas = (WORD*)(mapped_base + export_dir->AddressOfNameOrdinals);
	// Machine code address (RVA)
	DWORD* function_rvas = (DWORD*)(mapped_base + export_dir->AddressOfFunctions);

	void* func_GetHelloWorldString = NULL;
	void* func_CalculateAdd = NULL;

	// Traverse all directory
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

	// ----- 6. Test Functions -----

	if (func_GetHelloWorldString)
	{
		typedef const char* (*GetStrFunc)();
		GetStrFunc get_str = (GetStrFunc)func_GetHelloWorldString;
		printf("\n[EXEC] Calling GetHelloWorldString...\n");
		printf("[OUT]  %s\n", get_str());
	}
	else
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

	// ----- 7. Cleanup -----

	munmap(mapped_base, image_size);
	free(raw_data);
	printf("\n[*] Done.\n");
	return 0;
}