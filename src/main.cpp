/**
 * @file src/main.cpp
 * @author @SwordofMorning
 * @brief GFL Unix Loader
 * @version 0.1
 * @date 2026-05-08
 * 
 * @copyright Copyright (c) 2026
 * 
 */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <signal.h>
#include <ucontext.h>

#include <asm/prctl.h>
#include <sys/syscall.h>

#include "autogen_stubs.h"

// =========================================================================
// PE64 Structures (1-byte packing)
// =========================================================================
typedef uint8_t BYTE;
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

typedef struct _IMAGE_IMPORT_DESCRIPTOR
{
	union
	{
		DWORD Characteristics;
		DWORD OriginalFirstThunk;
	} DUMMYUNIONNAME;
	DWORD TimeDateStamp;
	DWORD ForwarderChain;
	DWORD Name;
	DWORD FirstThunk;
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
	char Name[1];
} IMAGE_IMPORT_BY_NAME;

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

typedef struct _IMAGE_BASE_RELOCATION
{
	DWORD VirtualAddress;
	DWORD SizeOfBlock;
} IMAGE_BASE_RELOCATION;
#pragma pack(pop)

// Global pointers for crash diagnostics
BYTE *g_mapped_base = NULL;
DWORD g_image_size = 0;

// =========================================================================
// Crash Handler (SIGSEGV)
// =========================================================================
void SegvHandler(int sig, siginfo_t *info, void *ucontext)
{
	ucontext_t *uc = (ucontext_t *)ucontext;

	// Extract CPU Instruction Pointer (RIP) and Stack Pointer (RSP)
	QWORD rip = uc->uc_mcontext.gregs[REG_RIP];
	QWORD rsp = uc->uc_mcontext.gregs[REG_RSP];
	QWORD rax = uc->uc_mcontext.gregs[REG_RAX];
	QWORD rcx = uc->uc_mcontext.gregs[REG_RCX];

	printf("\n==================== FATAL CRASH ====================\n");
	printf("[!] Caught SIGSEGV (Segmentation Fault)!\n");
	printf("	Faulting Memory Address : %p\n", info->si_addr);
	printf("	Instruction Pointer (RIP): 0x%lx\n", rip);
	printf("	Stack Pointer	   (RSP): 0x%lx\n", rsp);
	printf("	RAX: 0x%lx  |  RCX: 0x%lx\n", rax, rcx);

	if (g_mapped_base != NULL && rip >= (QWORD)g_mapped_base && rip < (QWORD)g_mapped_base + g_image_size)
	{
		printf("\n[*] Crash occurred INSIDE UnityPlayer.dll !\n");
		printf("	Offset from DLL Base: 0x%lx\n", rip - (QWORD)g_mapped_base);
	}
	else
	{
		printf("\n[*] Crash occurred OUTSIDE UnityPlayer.dll (maybe in Linux libs or our Thunks).\n");
	}
	printf("=====================================================\n");
	exit(1);
}

// =========================================================================
// Stubs & Calling Convention
// =========================================================================
// clang-format off
extern "C" void FatalMissingAPI(const char *func_name)
{
	printf("\n[!!! FATAL !!!] UnityPlayer attempted to call unimplemented API: %s\n", func_name);
	exit(1);
}

void *FindThunkByName(const char *name)
{
	for (int i = 0; g_auto_iat_hooks[i].name != 0; i++)
	{
		if (strcmp(g_auto_iat_hooks[i].name, name) == 0)
			return g_auto_iat_hooks[i].thunk_ptr;
	}
	return NULL;
}

extern "C" void CallWin64_DllMain(void *func_ptr, void *hInst, int reason, void *reserved)
{
	__asm__ volatile (
		"mov %1, %%rcx \n"
		"mov %2, %%edx \n"
		"mov %3, %%r8 \n"
		"sub $40, %%rsp \n"
		"call *%0 \n"
		"add $40, %%rsp \n"
		: : "r" (func_ptr), "r" (hInst), "r" (reason), "r" (reserved)
		: "rax", "rcx", "rdx", "r8", "r9", "r10", "r11", "memory"
	);
}

extern "C" void CallWin64_UnityMain(void *func_ptr, void *hInst, void *hPrev, void *cmdLine, int showCmd)
{
	__asm__ volatile (
		"mov %1, %%rcx \n"
		"mov %2, %%rdx \n"
		"mov %3, %%r8 \n"
		"mov %4, %%r9 \n"
		"sub $40, %%rsp \n"
		"call *%0 \n"
		"add $40, %%rsp \n"
		: : "r" (func_ptr), "r" (hInst), "r" (hPrev), "r" (cmdLine), "r" ((long long)showCmd)
		: "rax", "rcx", "rdx", "r8", "r9", "r10", "r11", "memory"
	);
}

void SetupFakeTEB()
{
	void *fake_teb = mmap(NULL, 4096, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	syscall(SYS_arch_prctl, ARCH_SET_GS, fake_teb);
	QWORD *peb_pointer = (QWORD*)((BYTE *)fake_teb + 0x30);
	*peb_pointer = (QWORD)fake_teb;
}

// clang-format on
// =========================================================================
// Main Loader logic
// =========================================================================
int main(int argc, char** argv)
{
	// Register the Crash Handler
	struct sigaction sa;
	sa.sa_flags = SA_SIGINFO;
	sigemptyset(&sa.sa_mask);
	sa.sa_sigaction = SegvHandler;
	sigaction(SIGSEGV, &sa, NULL);

	printf("[*] GFL Linux PE Loader Initializing...\n");

	if (argc < 2)
	{
		printf("Usage: %s <path_to_UnityPlayer.dll>\n", argv[0]);
		return 1;
	}

	SetupFakeTEB();

	// ----- 1. Read raw DLL file into memory -----

	int fd = open(argv[1], O_RDONLY);
	if (fd < 0)
	{
		perror("[-] Failed to open DLL");
		return 1;
	}

	struct stat st;
	fstat(fd, &st);
	BYTE *raw_data = (BYTE *)malloc(st.st_size);
	if (read(fd, raw_data, st.st_size) < 0)
	{
		close(fd);
		perror("[-] Failed to read fd");
		return 1;
	}
	close(fd);

	// ----- 2. Parse PE Headers -----

	// Read raw file
	IMAGE_DOS_HEADER *dos_header = (IMAGE_DOS_HEADER*)raw_data;
	// Offset the pointer to NT
	IMAGE_NT_HEADERS64 *nt_headers = (IMAGE_NT_HEADERS64*)(raw_data + dos_header->e_lfanew);

	// ----- 3. Allocate executable memory for the mapped image -----

	// To match the CPU's page alignment (typically 4KB), it will be stretched, thus we use `SizeOfImage`
	DWORD image_size = nt_headers->OptionalHeader.SizeOfImage;
	// MAP_ANONYMOUS zeroes out all allocated memory. This perfectly handles uninitialized data (.bss).
	BYTE *mapped_base = (BYTE *)mmap(NULL, image_size, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

	g_mapped_base = mapped_base;
	g_image_size = image_size;
	
	printf("[+] Allocated %u bytes at %p\n", image_size, mapped_base);

	// ----- 4. Map PE Headers -----

	// Copy NT image to mapped_base
	memcpy(mapped_base, raw_data, nt_headers->OptionalHeader.SizeOfHeaders);
	// Safely calculate Section Header starting point
	IMAGE_SECTION_HEADER *section = (IMAGE_SECTION_HEADER*)((BYTE *)nt_headers + 4 + sizeof(IMAGE_FILE_HEADER) + nt_headers->FileHeader.SizeOfOptionalHeader);

	// Traverse all sections
	for (int i = 0; i < nt_headers->FileHeader.NumberOfSections; i++)
	{
		if (section[i].SizeOfRawData > 0)
		{
			memcpy(mapped_base + section[i].VirtualAddress, raw_data + section[i].PointerToRawData, section[i].SizeOfRawData);
		}
	}

	// --- Process Base Relocations (Crucial for DLLs) ---
	QWORD delta = (QWORD)mapped_base - nt_headers->OptionalHeader.ImageBase;
	if (delta != 0)
	{
		DWORD reloc_rva = nt_headers->OptionalHeader.DataDirectory[5].VirtualAddress;
		if (reloc_rva != 0)
		{
			printf("[*] Applying Base Relocations (Delta: 0x%lx)...\n", delta);
			IMAGE_BASE_RELOCATION *reloc = (IMAGE_BASE_RELOCATION*)(mapped_base + reloc_rva);
			
			while (reloc->VirtualAddress != 0)
			{
				// Calculate number of entries in this block
				DWORD count = (reloc->SizeOfBlock - sizeof(IMAGE_BASE_RELOCATION)) / 2;
				WORD *list = (WORD*)(reloc + 1);
				
				for (DWORD i = 0; i < count; i++)
				{
					int type = list[i] >> 12;	  // Top 4 bits are the relocation type
					int offset = list[i] & 0x0FFF; // Bottom 12 bits are the offset
					
					if (type == 10)
					{ // IMAGE_REL_BASED_DIR64
						QWORD *patch_addr = (QWORD*)(mapped_base + reloc->VirtualAddress + offset);
						*patch_addr += delta;
					}
				}
				// Move to next block
				reloc = (IMAGE_BASE_RELOCATION*)((BYTE *)reloc + reloc->SizeOfBlock);
			}
			printf("[+] Relocations applied successfully.\n");
		}
	}

	// --- IAT Patching ---
	DWORD import_rva = nt_headers->OptionalHeader.DataDirectory[1].VirtualAddress;
	if (import_rva != 0)
	{
		IMAGE_IMPORT_DESCRIPTOR *import_desc = (IMAGE_IMPORT_DESCRIPTOR*)(mapped_base + import_rva);
		int hook_count = 0;
		while (import_desc->Name != 0)
		{
			IMAGE_THUNK_DATA64 *orig_thunk = (IMAGE_THUNK_DATA64*)(mapped_base + import_desc->DUMMYUNIONNAME.OriginalFirstThunk);
			IMAGE_THUNK_DATA64 *first_thunk = (IMAGE_THUNK_DATA64*)(mapped_base + import_desc->FirstThunk);

			for (int i = 0; orig_thunk[i].u1.AddressOfData != 0; i++)
			{
				if (orig_thunk[i].u1.Ordinal & 0x8000000000000000ULL)
				{
					char ord_buf[32];
					snprintf(ord_buf, sizeof(ord_buf), "Ordinal_%llu", (unsigned long long)(orig_thunk[i].u1.Ordinal & 0xFFFF));
					void *thunk = FindThunkByName(ord_buf);
					if(thunk) first_thunk[i].u1.Function = (QWORD)thunk;
				}
				else
				{
					IMAGE_IMPORT_BY_NAME *ibn = (IMAGE_IMPORT_BY_NAME*)(mapped_base + orig_thunk[i].u1.AddressOfData);
					void *thunk = FindThunkByName(ibn->Name);
					if (thunk)
					{
						first_thunk[i].u1.Function = (QWORD)thunk;
						hook_count++;
					}
				}
			}
			import_desc++;
		}
		printf("[+] Hooked %d API calls.\n", hook_count);
	}

	// --- Initialize DLL (DllMain) ---
	DWORD ep_rva = nt_headers->OptionalHeader.AddressOfEntryPoint;
	if (ep_rva != 0)
	{
		void *dll_main = mapped_base + ep_rva;
		printf("[EXEC] Invoking DllMain (DLL_PROCESS_ATTACH)...\n");
		// Windows API: BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved);
		// Reason 1 = DLL_PROCESS_ATTACH
		CallWin64_DllMain(dll_main, mapped_base, 1, NULL);
		printf("[OUT] DllMain Initialized Successfully.\n");
	}

	// --- Find and Call UnityMain ---
	DWORD export_rva = nt_headers->OptionalHeader.DataDirectory[0].VirtualAddress;
	void *func_UnityMain = NULL;
	if (export_rva != 0)
	{
		IMAGE_EXPORT_DIRECTORY *export_dir = (IMAGE_EXPORT_DIRECTORY*)(mapped_base + export_rva);
		DWORD *name_rvas = (DWORD*)(mapped_base + export_dir->AddressOfNames);
		WORD *ordinal_rvas = (WORD*)(mapped_base + export_dir->AddressOfNameOrdinals);
		DWORD *function_rvas = (DWORD*)(mapped_base + export_dir->AddressOfFunctions);

		for (DWORD i = 0; i < export_dir->NumberOfNames; i++)
		{
			char *func_name = (char*)(mapped_base + name_rvas[i]);
			if (strcmp(func_name, "UnityMain") == 0)
			{
				func_UnityMain = mapped_base + function_rvas[ordinal_rvas[i]];
				break;
			}
		}
	}

	if (func_UnityMain)
	{
		printf("\n[EXEC] Invoking UnityMain...\n");
		const wchar_t *cmdline = L"-force-vulkan";
		CallWin64_UnityMain(func_UnityMain, mapped_base, NULL, (void*)cmdline, 1);
		printf("\n[OUT] Execution Returned.\n");
	}
	else
	{
		printf("\n[-] Could not find UnityMain.\n");
	}

	munmap(mapped_base, image_size);
	free(raw_data);
	return 0;
}