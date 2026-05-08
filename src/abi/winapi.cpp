/**
 * @file src/abi/winapi.cpp
 * @author @SwordofMorning
 * @brief Implementations of Windows APIs on Linux
 * @version 0.1
 * @date 2026-05-08
 * 
 * @copyright Copyright (c) 2026
 * 
 */

#include "winapi.h"

// Using Linux native thread-local storage to emulate Windows TEB LastError
static thread_local DWORD g_last_error = 0;

/**
 * @section I: Utility
 */

/**
 * @subsection 1.1 UTF-16 to ASCII String conversion
 */
static void WcharToAscii(const uint16_t* wstr, char* out_buf, size_t max_len)
{
	size_t i = 0;
	while (wstr[i] != 0 && i < max_len - 1)
	{
		// Just cast down to char. Fine for pure ASCII paths like "d3d11.dll"
		out_buf[i] = (char)(wstr[i] & 0xFF); 
		i++;
	}
	out_buf[i] = '\0';
}

/**
 * @subsection 1.2 Virtual Module Tracker
 */
typedef struct
{
	char name[128];
	void* handle;
} VIRTUAL_MODULE;

#define MAX_VIRTUAL_MODULES 64
static VIRTUAL_MODULE g_virtual_modules[MAX_VIRTUAL_MODULES];
static int g_virtual_module_count = 0;

static void* GetOrRegisterVirtualModule(const char* name)
{
	// 1. Check if already loaded
	for (int i = 0; i < g_virtual_module_count; i++)
	{
		if (strcasecmp(g_virtual_modules[i].name, name) == 0)
		{
			return g_virtual_modules[i].handle;
		}
	}

	// 2. Register new virtual module
	if (g_virtual_module_count < MAX_VIRTUAL_MODULES)
	{
		// Generate a fake handle address (e.g., 0x1000, 0x1001...)
		void* fake_handle = (void*)(uintptr_t)(0x1000 + g_virtual_module_count);
		strncpy(g_virtual_modules[g_virtual_module_count].name, name, 127);
		g_virtual_modules[g_virtual_module_count].handle = fake_handle;
		g_virtual_module_count++;
		return fake_handle;
	}

	return NULL; // Tracker full
}

/**
 * 
 * @section II: C++ Implementations 
 * 
 * @note Windows x64 ABI
 * 1. Parameter registers (sequential parameter passing): 
 *	  Int/Pointer: RCX, RDX, R8, R9;
 *	  Float/Vector: XMM0, XMM1, XMM2, XMM3;
 *	  More than 4, use a stack to pass them.
 * 2. Return value registers:
 *	  2.1 RAX: Integer/pointer
 *	  2.2 XMM0: Float
 * 3. Volatile registers
 * The function can be modified at will; the caller must protect it themselves.
 *	  3.1 RAX, RCX, RDX, R8, R9, R10, R11
 *	  3.2 XMM0–XMM5
 * 4. Non-volatile registers
 * The function must be saved before use and restored before returning.
 *	  4.1 RBX, RBP, RDI, RSI, RSP, R12, R13, R14, R15
 *	  4.2 XMM6–XMM15
 * 5. Special registers
 *	  5.1 RSP: Stack
 *	  5.2 RBP: Base
 *	  5.3 RIP: Instruction
 *	  5.4 RFLAGS: Status Flags
 *	  5.5 XMM: Float/Vector
 * @attention Shadow Space:
 *	  "sub $32, %%rsp \n"			// [Win64 ABI] Allocate 32 bytes of shadow space
 *	  "call *%1 \n"			   // Call the Windows function pointer
 *	  "add $32, %%rsp \n"			// Clean up the shadow space
 * Windows requires that the caller reserve 32 bytes (the space of four 64-bit registers, or 0x20) of shadow space 
 * on the stack before making a call instruction, regardless of whether the function has parameters. 
 * Additionally, the RSP must be 16-byte aligned before the call is executed.
 *
 * @note System V AMD64 ABI
 * 1. Integer/pointer parameter registers (sequential parameter passing): 
 *	  RDI, RSI, RDX, RCX, R8, R9
 *	  More than 6, use a stack to pass them.
 * 2. Float parameter registers (sequential parameter passing): 
 *	  XMM0–XMM7
 * 3. Return value registers:
 *	  3.1 RAX: Integer/pointer
 *	  3.2 RDX: Helper, the high 64 bits of the 128-bit return value
 *	  3.3 XMM0: Float/Vector
 *	  3.4 XMM1: Helper
 * 4. Volatile registers
 *	  4.1 RAX, RCX, RDX, RSI, RDI, R8, R9, R10, R11
 *	  4.2 XMM0–XMM15, all
 *	  4.3 ST0–ST7 (X87 FPU)
 * 5. Non-volatile registers
 *	  5.1 RBX, RBP, R12, R13, R14, R15
 *	  5.2 RIP, keep alignment
 * 6. Special registers
 *	  6.1 RAX: Also used for system call numbers/return values
 *	  6.2 R10: Replaces RCX during system calls (because syscalls would break RCX/R11)
 *	  6.3 R11: Save RFLAGS during system call
 *	  6.4 RSP: Stack pointer, must be 16-byte aligned when entering/exiting functions
 *	  6.5 RBP: Base
 * 
 */

// Windows API: void GetSystemTimeAsFileTime(LPFILETIME lpSystemTimeAsFileTime)
WIN_API void Impl_GetSystemTimeAsFileTime(DWORD* lpSystemTimeAsFileTime)
{
	struct timespec ts;
	clock_gettime(CLOCK_REALTIME, &ts);

	// 1601 to 1970 is 11644473600 seconds
	QWORD filetime = ((QWORD)ts.tv_sec + 11644473600ULL) * 10000000ULL;
	filetime += (QWORD)ts.tv_nsec / 100ULL;

	// FILETIME is a struct of two DWORDs (Low, High)
	lpSystemTimeAsFileTime[0] = (DWORD)(filetime & 0xFFFFFFFF);
	lpSystemTimeAsFileTime[1] = (DWORD)(filetime >> 32);

	printf("	[API] Called GetSystemTimeAsFileTime (Unix TS: %lu)\n", ts.tv_sec);
}

// Windows API: DWORD GetCurrentThreadId(void)
WIN_API DWORD Impl_GetCurrentThreadId()
{
	pid_t tid = syscall(SYS_gettid);
	printf("	[API] Called GetCurrentThreadId (TID: %u)\n", (unsigned int)tid);
	return (DWORD)tid;
}

// Windows API: DWORD GetCurrentProcessId(void)
WIN_API DWORD Impl_GetCurrentProcessId()
{
	// getpid() is a standard POSIX function provided by unistd.h
	pid_t pid = getpid();
	printf("	[API] Called GetCurrentProcessId (PID: %u)\n", (unsigned int)pid);
	return (DWORD)pid;
}

// Windows API: BOOL QueryPerformanceCounter(LARGE_INTEGER *lpPerformanceCount)
WIN_API DWORD Impl_QueryPerformanceCounter(QWORD* lpPerformanceCount)
{
	struct timespec ts;
	// Keep monotonically increasing
	clock_gettime(CLOCK_MONOTONIC, &ts);

	// Convert to nanoseconds as the counter value
	// Windows QPC frequency is typically 10 MHz (100 ns ticks)
	// Use nanosecond precision here for simplicity
	QWORD counter = (QWORD)ts.tv_sec * 1000000000ULL + (QWORD)ts.tv_nsec;

	*lpPerformanceCount = counter;

	printf("	[API] Called QueryPerformanceCounter (Value: %lu)\n", counter);
	return 1; // TRUE
}

// Windows API: BOOL QueryPerformanceFrequency(LARGE_INTEGER *lpFrequency)
WIN_API DWORD Impl_QueryPerformanceFrequency(QWORD* lpFrequency)
{
	// Matches the nanosecond precision in QueryPerformanceCounter
	*lpFrequency = 1000000000ULL; 
	printf("	[API] Called QueryPerformanceFrequency (1 GHz)\n");
	return 1; // TRUE
}

// Windows API: HMODULE LoadLibraryExW(LPCWSTR lpLibFileName, HANDLE hFile, DWORD dwFlags)
WIN_API void* Impl_LoadLibraryExW(const uint16_t* lpLibFileName, void* hFile, DWORD dwFlags)
{
	char lib_name[256];
	WcharToAscii(lpLibFileName, lib_name, sizeof(lib_name));

	void* hModule = GetOrRegisterVirtualModule(lib_name);

	printf("	[API] Called LoadLibraryExW\n");
	printf("		-> Requested DLL : %s\n", lib_name);
	printf("		-> Assigned Handle: %p\n", hModule);

	return hModule;
}

// Windows API: FARPROC GetProcAddress(HMODULE hModule, LPCSTR lpProcName)
WIN_API void* Impl_GetProcAddress(void* hModule, const char* lpProcName)
{
	// lpProcName can be either a string pointer or an Ordinal number.
	// If the top 48 bits are zero, it's an ordinal.
	if ((uint64_t)lpProcName <= 0xFFFF)
	{
		char ord_buf[32];
		snprintf(ord_buf, sizeof(ord_buf), "Ordinal_%llu", (unsigned long long)(uintptr_t)lpProcName);
		
		void* func = FindRealThunkByName(ord_buf);
		if (!func) func = FindThunkByName(ord_buf);
		
		printf("	[API] GetProcAddress (Ordinal: %s) -> %p\n", ord_buf, func);
		return func;
	}

	// It's a normal string
	void* func = FindRealThunkByName(lpProcName);
	if (!func) func = FindThunkByName(lpProcName);

	if (func)
	{
		printf("	[API] GetProcAddress (Name: %s) -> %p\n", lpProcName, func);
	}
	else
	{
		printf("	[!!!] GetProcAddress FAILED to find: %s\n", lpProcName);
	}

	return func;
}

// Windows API: BOOL InitializeCriticalSectionEx(LPCRITICAL_SECTION lpCriticalSection, DWORD dwSpinCount, DWORD Flags)
WIN_API DWORD Impl_InitializeCriticalSectionEx(void* lpCriticalSection, DWORD dwSpinCount, DWORD Flags)
{
	pthread_mutexattr_t attr;
	pthread_mutexattr_init(&attr);
	
	// CRITICAL REQUIREMENT: Windows critical sections are recursive by default.
	pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
	
	// We cast the 40-byte Windows struct directly to a 40-byte Linux pthread_mutex_t
	pthread_mutex_init((pthread_mutex_t*)lpCriticalSection, &attr);
	pthread_mutexattr_destroy(&attr);

	printf("	[API] InitializeCriticalSectionEx (%p)\n", lpCriticalSection);
	return 1; // TRUE
}

// Windows API: void EnterCriticalSection(LPCRITICAL_SECTION lpCriticalSection)
WIN_API void Impl_EnterCriticalSection(void* lpCriticalSection)
{
	pthread_mutex_lock((pthread_mutex_t*)lpCriticalSection);
	// Note: We don't print here to avoid console spam, as this is called millions of times per second in a game.
}

// Windows API: void LeaveCriticalSection(LPCRITICAL_SECTION lpCriticalSection)
WIN_API void Impl_LeaveCriticalSection(void* lpCriticalSection)
{
	pthread_mutex_unlock((pthread_mutex_t*)lpCriticalSection);
}

// Windows API: void DeleteCriticalSection(LPCRITICAL_SECTION lpCriticalSection)
WIN_API void Impl_DeleteCriticalSection(void* lpCriticalSection)
{
	pthread_mutex_destroy((pthread_mutex_t*)lpCriticalSection);
	printf("	[API] DeleteCriticalSection (%p)\n", lpCriticalSection);
}

// Windows API: BOOL FreeLibrary(HMODULE hLibModule)
WIN_API DWORD Impl_FreeLibrary(void* hLibModule)
{
	printf("	[API] Called FreeLibrary (Handle: %p)\n", hLibModule);

	// Search for the module in our virtual module tracker
	for (int i = 0; i < g_virtual_module_count; i++)
	{
		if (g_virtual_modules[i].handle == hLibModule)
		{
			printf("		-> Unloading virtual module: %s\n", g_virtual_modules[i].name);

			// Remove by shifting remaining entries down
			for (int j = i; j < g_virtual_module_count - 1; j++)
			{
				g_virtual_modules[j] = g_virtual_modules[j + 1];
			}
			g_virtual_module_count--;

			return 1; // TRUE
		}
	}

	// Not found in our tracker; not necessarily an error
	printf("		-> Module handle %p not found in virtual module tracker\n", hLibModule);
	return 1; // Still return TRUE for compatibility
}

#define TLS_OUT_OF_INDEXES ((DWORD)0xFFFFFFFF)

// Windows API: DWORD TlsAlloc(void)
WIN_API DWORD Impl_TlsAlloc()
{
	pthread_key_t key;
	if (pthread_key_create(&key, NULL) == 0)
	{
		printf("	[API] TlsAlloc -> Index: %u\n", (DWORD)key);
		return (DWORD)key;
	}
	return TLS_OUT_OF_INDEXES;
}

// Windows API: LPVOID TlsGetValue(DWORD dwTlsIndex)
WIN_API void* Impl_TlsGetValue(DWORD dwTlsIndex)
{
	// Do not print here, as it's called very frequently in tight loops
	return pthread_getspecific((pthread_key_t)dwTlsIndex);
}

// Windows API: BOOL TlsSetValue(DWORD dwTlsIndex, LPVOID lpTlsValue)
WIN_API DWORD Impl_TlsSetValue(DWORD dwTlsIndex, void* lpTlsValue)
{
	// Do not print here either
	if (pthread_setspecific((pthread_key_t)dwTlsIndex, lpTlsValue) == 0)
	{
		return 1; // TRUE
	}
	return 0; // FALSE
}

// Windows API: BOOL TlsFree(DWORD dwTlsIndex)
WIN_API DWORD Impl_TlsFree(DWORD dwTlsIndex)
{
	if (pthread_key_delete((pthread_key_t)dwTlsIndex) == 0)
	{
		printf("	[API] TlsFree -> Index: %u\n", dwTlsIndex);
		return 1; // TRUE
	}
	return 0; // FALSE
}

// Windows API: HANDLE GetProcessHeap(void)
WIN_API void* Impl_GetProcessHeap()
{
	static void* process_heap_handle = PROCESS_HEAP_MAGIC;
	printf("	[API] Called GetProcessHeap -> Handle: %p\n", process_heap_handle);
	return process_heap_handle;
}

// Windows API: DWORD GetLastError(void)
WIN_API DWORD Impl_GetLastError()
{
	// Do not print here. GetLastError is often called in tight loops
	// to check status, printing will flood the console and drop FPS to 0.
	return g_last_error;
}

// Windows API: void SetLastError(DWORD dwErrCode)
WIN_API void Impl_SetLastError(DWORD dwErrCode)
{
	g_last_error = dwErrCode;
	
	/**
	 * @note Debug print
	 */
	printf("	[API] SetLastError: %u\n", dwErrCode);
}

// Windows API: LPVOID HeapAlloc(HANDLE hHeap, DWORD dwFlags, SIZE_T dwBytes)
WIN_API void* Impl_HeapAlloc(void* hHeap, DWORD dwFlags, size_t dwBytes)
{
	void* ptr = NULL;

	if (hHeap == PROCESS_HEAP_MAGIC)
	{
		if (dwFlags & HEAP_ZERO_MEMORY)
		{
			ptr = calloc(1, dwBytes);
		}
		else
		{
			ptr = malloc(dwBytes);
		}

		static bool logged_first_heap_alloc = false;
		if (!logged_first_heap_alloc)
		{
			printf("	[API] HeapAlloc (ProcessHeap, Size: %zu) -> %p\n", dwBytes, ptr);
			logged_first_heap_alloc = true;
		}
	}
	return ptr;
}

// Windows API: BOOL HeapFree(HANDLE hHeap, DWORD dwFlags, LPVOID lpMem)
WIN_API DWORD Impl_HeapFree(void* hHeap, DWORD dwFlags, void* lpMem)
{
	if (hHeap == PROCESS_HEAP_MAGIC)
	{
		free(lpMem);
		// Do not print here, it is called too frequently
		return 1; // TRUE
	}
	return 0; // FALSE
}

// Windows API: VOID GetStartupInfoW(LPSTARTUPINFOW lpStartupInfo)
WIN_API void Impl_GetStartupInfoW(STARTUPINFOW* lpStartupInfo)
{
	if (lpStartupInfo)
	{
		// Zero out the entire structure (Default behavior: no special window/handles)
		memset(lpStartupInfo, 0, sizeof(STARTUPINFOW));
		
		// Windows requires the 'cb' field to be set to the size of the structure.
		// On 64-bit Windows, this is exactly 104 bytes.
		lpStartupInfo->cb = sizeof(STARTUPINFOW);
	}
	printf("	[API] Called GetStartupInfoW\n");
}

// Windows API: HANDLE GetStdHandle(DWORD nStdHandle)
WIN_API void* Impl_GetStdHandle(DWORD nStdHandle)
{
	void* result = NULL;

	switch (nStdHandle)
	{
		case STD_INPUT_HANDLE:
			result = (void*)((uintptr_t)STD_HANDLE_BASE + 0);
			printf("	[API] Called GetStdHandle (STD_INPUT_HANDLE) -> %p\n", result);
			break;
		case STD_OUTPUT_HANDLE:
			result = (void*)((uintptr_t)STD_HANDLE_BASE + 1);
			printf("	[API] Called GetStdHandle (STD_OUTPUT_HANDLE) -> %p\n", result);
			break;
		case STD_ERROR_HANDLE:
			result = (void*)((uintptr_t)STD_HANDLE_BASE + 2);
			printf("	[API] Called GetStdHandle (STD_ERROR_HANDLE) -> %p\n", result);
			break;
		default:
			printf("	[!!!] GetStdHandle called with unknown handle type: %u\n", (unsigned int)nStdHandle);
			g_last_error = 87; // ERROR_INVALID_PARAMETER
			result = (void*)(uintptr_t)-1; // INVALID_HANDLE_VALUE
			break;
	}

	return result;
}

// Windows API: DWORD GetFileType(HANDLE hFile)
WIN_API DWORD Impl_GetFileType(void* hFile)
{
	uintptr_t handle_val = (uintptr_t)hFile;
	DWORD result = FILE_TYPE_UNKNOWN;

	// Check if it's one of our standard handles
	if (handle_val >= (uintptr_t)STD_HANDLE_BASE &&
		handle_val <= (uintptr_t)STD_HANDLE_BASE + 2)
	{
		// Standard I/O handles are character devices (terminal)
		result = FILE_TYPE_CHAR;
	}

	printf("	[API] Called GetFileType (Handle: %p) -> Type: %u\n", hFile, result);
	return result;
}

// Windows API: LPSTR GetCommandLineA(void)
WIN_API char* Impl_GetCommandLineA()
{
	static char cmdline[4096] = {0};
	static bool initialized = false;

	if (!initialized)
	{
		initialized = true;

		// Read command line from /proc/self/cmdline
		int fd = open("/proc/self/cmdline", O_RDONLY);
		if (fd >= 0)
		{
			ssize_t bytes = read(fd, cmdline, sizeof(cmdline) - 1);
			close(fd);

			if (bytes > 0)
			{
				cmdline[bytes] = '\0';

				// Replace null separators with spaces (except the last one)
				for (ssize_t i = 0; i < bytes - 1; i++)
				{
					if (cmdline[i] == '\0')
					{
						cmdline[i] = ' ';
					}
				}
			}
			else
			{
				// Fallback: just use program name
				strncpy(cmdline, "UnityPlayer", sizeof(cmdline) - 1);
			}
		}
		else
		{
			// Fallback: just use program name
			strncpy(cmdline, "UnityPlayer", sizeof(cmdline) - 1);
		}
	}

	printf("	[API] Called GetCommandLineA -> \"%s\"\n", cmdline);
	return cmdline;
}

WIN_API uint16_t* Impl_GetCommandLineW()
{
    static uint16_t wcmdline[4096] = {0};
    static bool initialized = false;

    if (!initialized)
    {
        initialized = true;
        char* cl_a = Impl_GetCommandLineA();
        // Simple ASCII to UTF-16 conversion
        for (size_t i = 0; cl_a[i] != '\0' && i < 4095; i++)
        {
            wcmdline[i] = (uint16_t)(unsigned char)cl_a[i];
        }
    }

    printf("    [API] Called GetCommandLineW\n");
    return wcmdline;
}

/**
 * @section III: Hook Table for Loader
 */

const REAL_API_ENTRY g_real_api_hooks[] =
{
	{"GetSystemTimeAsFileTime", (void*)Impl_GetSystemTimeAsFileTime},
	{"GetCurrentThreadId", (void*)Impl_GetCurrentThreadId},
	{"GetCurrentProcessId", (void*)Impl_GetCurrentProcessId},
	{"QueryPerformanceCounter", (void*)Impl_QueryPerformanceCounter},
	{"QueryPerformanceFrequency", (void*)Impl_QueryPerformanceFrequency},
	{"LoadLibraryExW", (void*)Impl_LoadLibraryExW},
	{"GetProcAddress", (void*)Impl_GetProcAddress},
	{"InitializeCriticalSectionEx", (void*)Impl_InitializeCriticalSectionEx},
	{"EnterCriticalSection", (void*)Impl_EnterCriticalSection},
	{"LeaveCriticalSection", (void*)Impl_LeaveCriticalSection},
	{"DeleteCriticalSection", (void*)Impl_DeleteCriticalSection},
	{"FreeLibrary", (void*)Impl_FreeLibrary},
	{"TlsAlloc", (void*)Impl_TlsAlloc},
	{"TlsGetValue", (void*)Impl_TlsGetValue},
	{"TlsSetValue", (void*)Impl_TlsSetValue},
	{"TlsFree", (void*)Impl_TlsFree},
	{"GetProcessHeap", (void*)Impl_GetProcessHeap},
	{"GetLastError", (void*)Impl_GetLastError},
	{"SetLastError", (void*)Impl_SetLastError},
	{"HeapAlloc", (void*)Impl_HeapAlloc},
	{"HeapFree", (void*)Impl_HeapFree},
	{"GetStartupInfoW", (void*)Impl_GetStartupInfoW},
	{"GetStdHandle", (void*)Impl_GetStdHandle},
	{"GetFileType", (void*)Impl_GetFileType},
	{"GetCommandLineA", (void*)Impl_GetCommandLineA},
	{"GetCommandLineW", (void*)Impl_GetCommandLineW},
	{0, 0} // Terminator
};