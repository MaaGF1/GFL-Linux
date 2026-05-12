#pragma once

#include "type.h"

#ifdef __cplusplus
extern "C" {
#endif

// Windows heap flags
#define HEAP_NO_SERIALIZE			0x00000001
#define HEAP_GENERATE_EXCEPTIONS	0x00000004
#define HEAP_ZERO_MEMORY			0x00000008
#define PROCESS_HEAP_MAGIC			((void*)(uintptr_t)0xDEAD0000)

// Windows 64-bit STARTUPINFOW Structure
typedef struct _STARTUPINFOW
{
	DWORD cb;				// 4 bytes
	DWORD _pad1;			// 4 bytes padding for 64-bit alignment
	uint16_t *lpReserved;	// 8 bytes
	uint16_t *lpDesktop;	// 8 bytes
	uint16_t *lpTitle;		// 8 bytes
	DWORD dwX;				// 4 bytes
	DWORD dwY;				// 4 bytes
	DWORD dwXSize;			// 4 bytes
	DWORD dwYSize;			// 4 bytes
	DWORD dwXCountChars;	// 4 bytes
	DWORD dwYCountChars;	// 4 bytes
	DWORD dwFillAttribute;	// 4 bytes
	DWORD dwFlags;			// 4 bytes
	WORD  wShowWindow;		// 2 bytes
	WORD cbReserved2;		// 2 bytes
	DWORD _pad2;			// 4 bytes padding
	uint8_t *lpReserved2;	// 8 bytes
	void *hStdInput;		// 8 bytes
	void *hStdOutput;		// 8 bytes
	void *hStdError;		// 8 bytes
} STARTUPINFOW;

// Windows standard device Flags
#define STD_INPUT_HANDLE    ((DWORD)-10)
#define STD_OUTPUT_HANDLE   ((DWORD)-11)
#define STD_ERROR_HANDLE    ((DWORD)-12)
#define STD_HANDLE_BASE     ((void*)(uintptr_t)0xF0000000)

// Windows file types
#define FILE_TYPE_UNKNOWN   0x0000
#define FILE_TYPE_DISK      0x0001
#define FILE_TYPE_CHAR      0x0002
#define FILE_TYPE_PIPE      0x0003
#define FILE_TYPE_REMOTE    0x8000

typedef union _SLIST_HEADER
{
    QWORD Alignment;
    struct
	{
        QWORD Depth : 16;
        QWORD Sequence : 48;
        QWORD Reserved : 4;
        QWORD NextEntry : 60;
    } HeaderX64;
} SLIST_HEADER, *PSLIST_HEADER;

#define TLS_OUT_OF_INDEXES ((DWORD)0xFFFFFFFF)

// =========================================================================
// Processor Features (for IsProcessorFeaturePresent)
// =========================================================================
#define PF_FLOATING_POINT_PRECISION_ERRATA  0
#define PF_FLOATING_POINT_EMULATED          1
#define PF_COMPARE_EXCHANGE_DOUBLE          2  // cmpxchg8b
#define PF_MMX_INSTRUCTIONS_AVAILABLE       3
#define PF_XMMI_INSTRUCTIONS_AVAILABLE      6  // SSE
#define PF_3DNOW_INSTRUCTIONS_AVAILABLE     7
#define PF_RDTSC_INSTRUCTION_AVAILABLE      8
#define PF_PAE_ENABLED                      9
#define PF_XMMI64_INSTRUCTIONS_AVAILABLE    10 // SSE2
#define PF_SSE_DAZ_MODE_AVAILABLE           11
#define PF_NX_ENABLED                       12 // Data Execution Prevention
#define PF_SSE3_INSTRUCTIONS_AVAILABLE      13
#define PF_COMPARE_EXCHANGE128              14 // cmpxchg16b
#define PF_COMPARE64_EXCHANGE128            15
#define PF_CHANNELS_ENABLED                 16
#define PF_XSAVE_ENABLED                    17
#define PF_ARM_VFP_32_REGISTERS_AVAILABLE   18
#define PF_ARM_NEON_INSTRUCTIONS_AVAILABLE  19
#define PF_SECOND_LEVEL_ADDRESS_TRANSLATION 20
#define PF_VIRT_FIRMWARE_ENABLED            21
#define PF_RDWRFSGSBASE_AVAILABLE           22
#define PF_FASTFAIL_AVAILABLE               23
#define PF_ARM_DIVIDE_INSTRUCTION_AVAILABLE 24
#define PF_ARM_64BIT_LOADSTORE_ATOMIC       25
#define PF_ARM_EXTERNAL_CACHE_AVAILABLE     26
#define PF_ARM_FMAC_INSTRUCTIONS_AVAILABLE  27

// =========================================================================
// Windows Virtual Memory Constants & Structures
// =========================================================================

// Windows Page Protection Flags
#define PAGE_NOACCESS          0x01
#define PAGE_READONLY          0x02
#define PAGE_READWRITE         0x04
#define PAGE_WRITECOPY         0x08
#define PAGE_EXECUTE           0x10
#define PAGE_EXECUTE_READ      0x20
#define PAGE_EXECUTE_READWRITE 0x40
#define PAGE_EXECUTE_WRITECOPY 0x80

// Windows Allocation Type Flags
#define MEM_COMMIT             0x00001000
#define MEM_RESERVE            0x00002000
#define MEM_DECOMMIT           0x00004000
#define MEM_RELEASE            0x00008000
#define MEM_FREE               0x00010000
#define MEM_PRIVATE            0x00020000

// Exact 48-byte struct for Windows x64 ABI
typedef struct _MEMORY_BASIC_INFORMATION
{
    void*  BaseAddress;
    void*  AllocationBase;
    DWORD  AllocationProtect;
    DWORD  __alignment1;
    size_t RegionSize;
    DWORD  State;
    DWORD  Protect;
    DWORD  Type;
    DWORD  __alignment2;
} MEMORY_BASIC_INFORMATION;

#ifdef __cplusplus
}
#endif