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

#ifdef __cplusplus
}
#endif