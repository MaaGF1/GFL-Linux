/**
 * @file poc/helloworld_IAT/loader/src/main.cpp
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
// PE64 Structures (1-byte packing)
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
} IMAGE_SECTION_HEADER;

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

// --- IAT Structures ---

typedef struct _IMAGE_IMPORT_DESCRIPTOR
{
	union
	{
		DWORD Characteristics;
		DWORD OriginalFirstThunk; // INT
	} DUMMYUNIONNAME;
	DWORD TimeDateStamp;
	DWORD ForwarderChain;
	DWORD Name;                   // RVA to DLL name
	DWORD FirstThunk;             // IAT
} IMAGE_IMPORT_DESCRIPTOR;

typedef struct _IMAGE_THUNK_DATA64
{
	union
	{
		QWORD ForwarderString;
		QWORD Function;
		QWORD Ordinal;
		QWORD AddressOfData;
	} u1;
} IMAGE_THUNK_DATA64;

typedef struct _IMAGE_IMPORT_BY_NAME
{
	WORD Hint;
	char Name[1]; // Variable size
} IMAGE_IMPORT_BY_NAME;

#pragma pack(pop)

// =========================================================================
// Win64 to SysV ABI Thunk (Calling DLL from Linux)
// =========================================================================

extern "C" void CallWin64Func_0Args(void* func_ptr)
{
	__asm__ volatile (
		"sub $32, %%rsp \n"
		"call *%0 \n"
		"add $32, %%rsp \n"
		: 
		: "r" (func_ptr)
		: "rax", "rcx", "rdx", "r8", "r9", "r10", "r11", "memory"
	);
}

// =========================================================================
// Reverse ABI Thunks & Mock Linux Functions (Calling Linux from DLL)
// =========================================================================

// 1. Mock Functions (SysV ABI)
extern "C" void MockOutputDebugStringA(const char* str)
{
	printf("\n[MOCK KERNEL32] OutputDebugStringA: %s\n", str);
}

extern "C" int MockMessageBoxA(void* hwnd, const char* text, const char* caption, unsigned int type)
{
	printf("\n[MOCK USER32] MessageBoxA Triggered!\n");
	printf("    -> HWND:    %p\n", hwnd);
	printf("    -> Caption: %s\n", caption);
	printf("    -> Text:    %s\n", text);
	printf("    -> Type:    %u\n", type);
	return 1; // IDOK
}

extern "C" void MockUnhandledImport()
{
	printf("\n[-] WARNING: Unhandled Import Called!\n");
}

// 2. Assembly Trampolines (Reverse Win64 -> SysV)
// Using GCC global assembly blocks

// Linux: RDI, RSI, RDX, RCX, R8, R9
// Windows: RCX(a), RDX(b), R8, R9
__asm__(
	".text\n"
	".global Thunk_OutputDebugStringA\n"
	"Thunk_OutputDebugStringA:\n"
	"    mov %rcx, %rdi\n"				// Win64 Arg 1 (RCX) -> SysV Arg 1 (RDI)
	"    jmp MockOutputDebugStringA\n"	// Tail call
);

__asm__(
	".text\n"
	".global Thunk_MessageBoxA\n"
	"Thunk_MessageBoxA:\n"
	"    mov %rcx, %rdi\n"         // Arg 1: HWND
	"    mov %rdx, %rsi\n"         // Arg 2: Text
	"    mov %r8, %rdx\n"          // Arg 3: Caption
	"    mov %r9, %rcx\n"          // Arg 4: Type
	"    jmp MockMessageBoxA\n"    // Tail call
);

__asm__(
	".text\n"
	".global Thunk_UnhandledImport\n"
	"Thunk_UnhandledImport:\n"
	"    jmp MockUnhandledImport\n"
);

extern "C" void Thunk_OutputDebugStringA();
extern "C" void Thunk_MessageBoxA();
extern "C" void Thunk_UnhandledImport();


// =========================================================================
// PE Loader Core
// =========================================================================

int main(int argc, char** argv)
{
	printf("[*] Linux PE Loader with IAT Hooking Started.\n");

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
	for (int i = 0; i < nt_headers->FileHeader.NumberOfSections; i++)
	{
		if (section[i].SizeOfRawData > 0)
		{
			memcpy(mapped_base + section[i].VirtualAddress, raw_data + section[i].PointerToRawData, section[i].SizeOfRawData);
		}
	}

	// ----- 5. Patch IAT (Import Address Table) -----
	DWORD import_rva = nt_headers->OptionalHeader.DataDirectory[1].VirtualAddress; // Index 1 is Import Dir
	if (import_rva == 0)
	{
		printf("[-] No Import Directory found.\n");
	}
	else
	{
		IMAGE_IMPORT_DESCRIPTOR* import_desc = (IMAGE_IMPORT_DESCRIPTOR*)(mapped_base + import_rva);
		printf("\n[*] Resolving Import Address Table (IAT)...\n");

		while (import_desc->Name != 0)
		{
			char* dll_name = (char*)(mapped_base + import_desc->Name);
			printf("    -> Loading Imports from: %s\n", dll_name);

			IMAGE_THUNK_DATA64* orig_thunk = (IMAGE_THUNK_DATA64*)(mapped_base + import_desc->DUMMYUNIONNAME.OriginalFirstThunk);
			IMAGE_THUNK_DATA64* first_thunk = (IMAGE_THUNK_DATA64*)(mapped_base + import_desc->FirstThunk);

			for (int i = 0; orig_thunk[i].u1.AddressOfData != 0; i++)
			{
				if (orig_thunk[i].u1.Ordinal & 0x8000000000000000ULL)
				{
					printf("        [!] Skipping Ordinal Import\n");
					first_thunk[i].u1.Function = (QWORD)Thunk_UnhandledImport;
				}
				else
				{
					IMAGE_IMPORT_BY_NAME* ibn = (IMAGE_IMPORT_BY_NAME*)(mapped_base + orig_thunk[i].u1.AddressOfData);
					char* func_name = ibn->Name;

					// Hijack logic
					if (strcmp(func_name, "OutputDebugStringA") == 0)
					{
						first_thunk[i].u1.Function = (QWORD)Thunk_OutputDebugStringA;
						printf("        [+] Hooked: %s -> %p\n", func_name, (void*)Thunk_OutputDebugStringA);
					}
					else if (strcmp(func_name, "MessageBoxA") == 0)
					{
						first_thunk[i].u1.Function = (QWORD)Thunk_MessageBoxA;
						printf("        [+] Hooked: %s -> %p\n", func_name, (void*)Thunk_MessageBoxA);
					}
					else
					{
						first_thunk[i].u1.Function = (QWORD)Thunk_UnhandledImport;
						printf("        [-] Ignored: %s\n", func_name);
					}
				}
			}
			import_desc++;
		}
	}

	// ----- 6. Find and Call Exported Function -----
	DWORD export_rva = nt_headers->OptionalHeader.DataDirectory[0].VirtualAddress;
	IMAGE_EXPORT_DIRECTORY* export_dir = (IMAGE_EXPORT_DIRECTORY*)(mapped_base + export_rva);
	DWORD* name_rvas = (DWORD*)(mapped_base + export_dir->AddressOfNames);
	WORD* ordinal_rvas = (WORD*)(mapped_base + export_dir->AddressOfNameOrdinals);
	DWORD* function_rvas = (DWORD*)(mapped_base + export_dir->AddressOfFunctions);

	void* func_TestIATCalls = NULL;

	for (DWORD i = 0; i < export_dir->NumberOfNames; i++)
	{
		char* func_name = (char*)(mapped_base + name_rvas[i]);
		if (strcmp(func_name, "TestIATCalls") == 0)
		{
			func_TestIATCalls = mapped_base + function_rvas[ordinal_rvas[i]];
			break;
		}
	}

	if (func_TestIATCalls)
	{
		printf("\n[EXEC] Calling TestIATCalls()...\n");
		CallWin64Func_0Args(func_TestIATCalls);
		printf("\n[OUT]  TestIATCalls() Execution Finished.\n");
	}
	else
	{
		printf("\n[-] Could not find TestIATCalls\n");
	}

	// ----- 7. Cleanup -----
	munmap(mapped_base, image_size);
	free(raw_data);
	printf("\n[*] Done.\n");
	return 0;
}