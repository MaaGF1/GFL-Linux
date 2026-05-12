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
// Access main module base address from main.cpp
extern BYTE *g_mapped_base;
// Get all environment variables in the current Linux Shell session
extern char **environ;

/**
 * ====================================================================================================
 * @section I: Utility
 * ====================================================================================================
 */

/**
 * @subsection 1.1 UTF-16 to ASCII String conversion
 */
static void WcharToAscii(const uint16_t *wstr, char *out_buf, size_t max_len)
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
	void *handle;
} VIRTUAL_MODULE;

#define MAX_VIRTUAL_MODULES 64
static VIRTUAL_MODULE g_virtual_modules[MAX_VIRTUAL_MODULES];
static int g_virtual_module_count = 0;

static void *GetOrRegisterVirtualModule(const char *name)
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
		void *fake_handle = (void *)(uintptr_t)(0x1000 + g_virtual_module_count);
		strncpy(g_virtual_modules[g_virtual_module_count].name, name, 127);
		g_virtual_modules[g_virtual_module_count].handle = fake_handle;
		g_virtual_module_count++;
		return fake_handle;
	}

	return NULL; // Tracker full
}

/**
 * @subsection 1.3 ASCII to UTF-16 String conversion (Helper)
 */
static void AsciiToWchar(const char *str, uint16_t *wstr, size_t max_chars)
{
	size_t i = 0;
	while (str[i] != '\0' && i < max_chars - 1)
	{
		wstr[i] = (uint16_t)(unsigned char)str[i];
		i++;
	}
	wstr[i] = 0;
}

/**
 * ====================================================================================================
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
 * ====================================================================================================
 */

// Windows API: void GetSystemTimeAsFileTime(LPFILETIME lpSystemTimeAsFileTime)
WIN_API void Impl_GetSystemTimeAsFileTime(DWORD *lpSystemTimeAsFileTime)
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
WIN_API DWORD Impl_QueryPerformanceCounter(QWORD *lpPerformanceCount)
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
WIN_API DWORD Impl_QueryPerformanceFrequency(QWORD *lpFrequency)
{
	// Matches the nanosecond precision in QueryPerformanceCounter
	*lpFrequency = 1000000000ULL;
	printf("	[API] Called QueryPerformanceFrequency (1 GHz)\n");
	return 1; // TRUE
}

// Windows API: HMODULE LoadLibraryExW(LPCWSTR lpLibFileName, HANDLE hFile, DWORD dwFlags)
WIN_API void *Impl_LoadLibraryExW(const uint16_t *lpLibFileName, void *hFile, DWORD dwFlags)
{
	char lib_name[256];
	WcharToAscii(lpLibFileName, lib_name, sizeof(lib_name));

	void *hModule = GetOrRegisterVirtualModule(lib_name);

	printf("	[API] Called LoadLibraryExW\n");
	printf("		-> Requested DLL : %s\n", lib_name);
	printf("		-> Assigned Handle: %p\n", hModule);

	return hModule;
}

// Windows API: FARPROC GetProcAddress(HMODULE hModule, LPCSTR lpProcName)
WIN_API void *Impl_GetProcAddress(void *hModule, const char *lpProcName)
{
	// lpProcName can be either a string pointer or an Ordinal number.
	// If the top 48 bits are zero, it's an ordinal.
	if ((uint64_t)lpProcName <= 0xFFFF)
	{
		char ord_buf[32];
		snprintf(ord_buf, sizeof(ord_buf), "Ordinal_%llu", (unsigned long long)(uintptr_t)lpProcName);

		void *func = FindRealThunkByName(ord_buf);
		if (!func)
			func = FindThunkByName(ord_buf);

		printf("	[API] GetProcAddress (Ordinal: %s) -> %p\n", ord_buf, func);
		return func;
	}

	// It's a normal string
	void *func = FindRealThunkByName(lpProcName);
	if (!func)
		func = FindThunkByName(lpProcName);

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
WIN_API DWORD Impl_InitializeCriticalSectionEx(void *lpCriticalSection, DWORD dwSpinCount, DWORD Flags)
{
	pthread_mutexattr_t attr;
	pthread_mutexattr_init(&attr);

	// CRITICAL REQUIREMENT: Windows critical sections are recursive by default.
	pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);

	// We cast the 40-byte Windows struct directly to a 40-byte Linux pthread_mutex_t
	pthread_mutex_init((pthread_mutex_t *)lpCriticalSection, &attr);
	pthread_mutexattr_destroy(&attr);

	printf("	[API] InitializeCriticalSectionEx (%p)\n", lpCriticalSection);
	return 1; // TRUE
}

// Windows API: void EnterCriticalSection(LPCRITICAL_SECTION lpCriticalSection)
WIN_API void Impl_EnterCriticalSection(void *lpCriticalSection)
{
	pthread_mutex_lock((pthread_mutex_t *)lpCriticalSection);
	// Note: We don't print here to avoid console spam, as this is called millions of times per second in a game.
}

// Windows API: void LeaveCriticalSection(LPCRITICAL_SECTION lpCriticalSection)
WIN_API void Impl_LeaveCriticalSection(void *lpCriticalSection)
{
	pthread_mutex_unlock((pthread_mutex_t *)lpCriticalSection);
}

// Windows API: void DeleteCriticalSection(LPCRITICAL_SECTION lpCriticalSection)
WIN_API void Impl_DeleteCriticalSection(void *lpCriticalSection)
{
	pthread_mutex_destroy((pthread_mutex_t *)lpCriticalSection);
	printf("	[API] DeleteCriticalSection (%p)\n", lpCriticalSection);
}

// Windows API: BOOL FreeLibrary(HMODULE hLibModule)
WIN_API DWORD Impl_FreeLibrary(void *hLibModule)
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
WIN_API void *Impl_TlsGetValue(DWORD dwTlsIndex)
{
	// Do not print here, as it's called very frequently in tight loops
	return pthread_getspecific((pthread_key_t)dwTlsIndex);
}

// Windows API: BOOL TlsSetValue(DWORD dwTlsIndex, LPVOID lpTlsValue)
WIN_API DWORD Impl_TlsSetValue(DWORD dwTlsIndex, void *lpTlsValue)
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
WIN_API void *Impl_GetProcessHeap()
{
	static void *process_heap_handle = PROCESS_HEAP_MAGIC;
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
WIN_API void *Impl_HeapAlloc(void *hHeap, DWORD dwFlags, size_t dwBytes)
{
	void *ptr = NULL;

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
WIN_API DWORD Impl_HeapFree(void *hHeap, DWORD dwFlags, void *lpMem)
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
WIN_API void Impl_GetStartupInfoW(STARTUPINFOW *lpStartupInfo)
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
WIN_API void *Impl_GetStdHandle(DWORD nStdHandle)
{
	void *result = NULL;

	// clang-format off
	switch (nStdHandle)
	{
		case STD_INPUT_HANDLE:
			result = (void *)((uintptr_t)STD_HANDLE_BASE + 0);
			printf("	[API] Called GetStdHandle (STD_INPUT_HANDLE) -> %p\n", result);
			break;
		case STD_OUTPUT_HANDLE:
			result = (void *)((uintptr_t)STD_HANDLE_BASE + 1);
			printf("	[API] Called GetStdHandle (STD_OUTPUT_HANDLE) -> %p\n", result);
			break;
		case STD_ERROR_HANDLE:
			result = (void *)((uintptr_t)STD_HANDLE_BASE + 2);
			printf("	[API] Called GetStdHandle (STD_ERROR_HANDLE) -> %p\n", result);
			break;
		default:
			printf("	[!!!] GetStdHandle called with unknown handle type: %u\n", (unsigned int)nStdHandle);
			g_last_error = 87; // ERROR_INVALID_PARAMETER
			result = (void *)(uintptr_t)-1; // INVALID_HANDLE_VALUE
			break;
	}
	// clang-format on

	return result;
}

// Windows API: DWORD GetFileType(HANDLE hFile)
WIN_API DWORD Impl_GetFileType(void *hFile)
{
	uintptr_t handle_val = (uintptr_t)hFile;
	DWORD result = FILE_TYPE_UNKNOWN;

	// Check if it's one of our standard handles
	if (handle_val >= (uintptr_t)STD_HANDLE_BASE && handle_val <= (uintptr_t)STD_HANDLE_BASE + 2)
	{
		// Standard I/O handles are character devices (terminal)
		result = FILE_TYPE_CHAR;
	}

	printf("	[API] Called GetFileType (Handle: %p) -> Type: %u\n", hFile, result);
	return result;
}

// Windows API: LPSTR GetCommandLineA(void)
WIN_API char *Impl_GetCommandLineA()
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

WIN_API uint16_t *Impl_GetCommandLineW()
{
	static uint16_t wcmdline[4096] = {0};
	static bool initialized = false;

	if (!initialized)
	{
		initialized = true;
		char *cl_a = Impl_GetCommandLineA();
		// Simple ASCII to UTF-16 conversion
		for (size_t i = 0; cl_a[i] != '\0' && i < 4095; i++)
		{
			wcmdline[i] = (uint16_t)(unsigned char)cl_a[i];
		}
	}

	printf("	[API] Called GetCommandLineW\n");
	return wcmdline;
}

// Windows API: UINT GetACP(void)
WIN_API DWORD Impl_GetACP()
{
	// By returning 65001 (UTF-8), we tell the Windows CRT to treat all standard 
	// "ANSI" strings as UTF-8. This matches Linux's native encoding perfectly!
	printf("	[API] Called GetACP -> Returning %d (CP_UTF8)\n", CP_UTF8);
	return CP_UTF8;
}

// Windows API: UINT GetOEMCP(void)
WIN_API DWORD Impl_GetOEMCP()
{
	// OEM Code Page (used for old console apps). Same as ACP for our purposes.
	printf("	[API] Called GetOEMCP -> Returning %d (CP_UTF8)\n", CP_UTF8);
	return CP_UTF8;
}

// Windows API: BOOL IsValidCodePage(UINT CodePage)
WIN_API DWORD Impl_IsValidCodePage(DWORD CodePage)
{
	// Pretend that whatever code page the engine asks for is valid, 
	// to prevent it from crashing or throwing exceptions.
	printf("	[API] Called IsValidCodePage (%u)\n", CodePage);
	return 1; // TRUE
}

// Windows API: void InitializeSListHead(PSLIST_HEADER ListHead)
WIN_API void Impl_InitializeSListHead(PSLIST_HEADER ListHead)
{
	if (ListHead)
	{
		memset(ListHead, 0, sizeof(SLIST_HEADER));
	}
	printf("	[API] InitializeSListHead (%p)\n", ListHead);
}

// Windows API: HMODULE GetModuleHandleW(LPCWSTR lpModuleName)
WIN_API void *Impl_GetModuleHandleW(const uint16_t *lpModuleName)
{
	// If lpModuleName is NULL, return the base address of the main module
	if (lpModuleName == NULL)
	{
		printf("	[API] GetModuleHandleW (NULL) -> Main Module %p\n", g_mapped_base);
		return g_mapped_base;
	}

	// Convert wide string to ASCII for lookup
	char module_name[256];
	WcharToAscii(lpModuleName, module_name, sizeof(module_name));

	// Search in virtual module tracker (for dynamically loaded libs)
	for (int i = 0; i < g_virtual_module_count; i++)
	{
		if (strcasecmp(g_virtual_modules[i].name, module_name) == 0)
		{
			printf("	[API] GetModuleHandleW (%s) -> Tracker Handle %p\n", module_name, g_virtual_modules[i].handle);
			return g_virtual_modules[i].handle;
		}
	}

	// [THE MAGIC HACK]
	// Core Windows libraries (kernel32, ntdll, etc.) are heavily assumed to be present.
	// The CRT WILL dereference the handle expecting a valid DOS/PE header ('MZ').
	// By returning g_mapped_base (which points to a valid PE file), we prevent NULL pointer crashes!
	printf("	[API] GetModuleHandleW (%s) -> Not found! Returning Universal Fake PE Handle %p\n", module_name, g_mapped_base);
	return g_mapped_base;
}


// Windows API: BOOL IsProcessorFeaturePresent(DWORD ProcessorFeature)
WIN_API DWORD Impl_IsProcessorFeaturePresent(DWORD ProcessorFeature)
{
	// Default to FALSE
	DWORD result = 0;

	switch (ProcessorFeature)
	{
		case PF_XMMI_INSTRUCTIONS_AVAILABLE:
		case PF_XMMI64_INSTRUCTIONS_AVAILABLE:
		case PF_NX_ENABLED:
			result = 1;
			break;
		default:
			// We forcefully disable PF_FASTFAIL_AVAILABLE (23) and AVX to prevent 
			// the CRT from taking aggressive hardware/security paths.
			result = 0;
			break;
	}

	printf("	[API] Called IsProcessorFeaturePresent (Feature: %u) -> %s\n", 
		   (unsigned int)ProcessorFeature, result ? "TRUE" : "FALSE");

	return result;
}

// Windows API: HANDLE CreateEventW(LPSECURITY_ATTRIBUTES lpEventAttributes, BOOL bManualReset, BOOL bInitialState, LPCWSTR lpName)
WIN_API void* Impl_CreateEventW(void* lpEventAttributes, DWORD bManualReset, DWORD bInitialState, const uint16_t* lpName)
{
	// Allocate our custom event structure
	WIN_EVENT* ev = (WIN_EVENT*)malloc(sizeof(WIN_EVENT));
	if (!ev) return NULL;

	ev->magic = EVENT_MAGIC;
	ev->manual_reset = bManualReset;
	ev->is_signaled = bInitialState;
	
	pthread_mutex_init(&ev->mutex, NULL);
	pthread_cond_init(&ev->cond, NULL);

	// Optional: Extract name for debugging if lpName is not NULL
	char name_buf[128] = "Unnamed";
	if (lpName) {
		WcharToAscii(lpName, name_buf, sizeof(name_buf));
	}

	printf("	[API] CreateEventW (Name: %s, Manual: %u, Signaled: %u) -> Handle: %p\n", 
		   name_buf, bManualReset, bInitialState, ev);
		   
	return ev;
}

// Windows API: BOOL SetEvent(HANDLE hEvent)
WIN_API DWORD Impl_SetEvent(void* hEvent)
{
	if (!hEvent) return 0;
	WIN_EVENT* ev = (WIN_EVENT*)hEvent;

	if (ev->magic == EVENT_MAGIC)
	{
		pthread_mutex_lock(&ev->mutex);
		ev->is_signaled = 1;
		
		if (ev->manual_reset) {
			// Wake ALL waiting threads
			pthread_cond_broadcast(&ev->cond);
		} else {
			// Wake ONLY ONE waiting thread (Auto-reset)
			pthread_cond_signal(&ev->cond);
		}
		pthread_mutex_unlock(&ev->mutex);
		
		// Note: SetEvent is called extremely often, better to keep printing disabled or minimal
		// printf("	[API] SetEvent (%p)\n", hEvent);
		return 1; // TRUE
	}
	return 0; // FALSE
}

// Windows API: BOOL ResetEvent(HANDLE hEvent)
WIN_API DWORD Impl_ResetEvent(void* hEvent)
{
	if (!hEvent) return 0;
	WIN_EVENT* ev = (WIN_EVENT*)hEvent;

	if (ev->magic == EVENT_MAGIC)
	{
		pthread_mutex_lock(&ev->mutex);
		ev->is_signaled = 0;
		pthread_mutex_unlock(&ev->mutex);
		return 1; // TRUE
	}
	return 0; // FALSE
}

// Windows API: BOOL CloseHandle(HANDLE hObject)
WIN_API DWORD Impl_CloseHandle(void* hObject)
{
	if (!hObject) return 1; // Sometimes they pass NULL, just return TRUE

	// 1. Check if it's one of our Events
	WIN_EVENT* ev = (WIN_EVENT*)hObject;
	if (ev->magic == EVENT_MAGIC)
	{
		// Destroy Linux primitives and free memory
		pthread_mutex_destroy(&ev->mutex);
		pthread_cond_destroy(&ev->cond);
		
		// Overwrite magic to prevent Use-After-Free bugs
		ev->magic = 0; 
		free(ev);
		
		printf("	[API] CloseHandle (Destroyed Event: %p)\n", hObject);
		return 1; // TRUE
	}

	// 2. Check if it's one of our standard output handles (stdin/stdout)
	uintptr_t handle_val = (uintptr_t)hObject;
	if (handle_val >= (uintptr_t)STD_HANDLE_BASE && handle_val <= (uintptr_t)STD_HANDLE_BASE + 2)
	{
		printf("	[API] CloseHandle (Ignored Standard I/O Handle: %p)\n", hObject);
		return 1; // TRUE
	}

	// If we don't know what it is, just pretend we closed it successfully.
	// In a mature loader, you'd maintain a global Handle Table.
	printf("	[API] CloseHandle (Unknown Handle: %p)\n", hObject);
	return 1; // TRUE
}

// Windows API: DWORD GetModuleFileNameW(HMODULE hModule, LPWSTR lpFilename, DWORD nSize)
WIN_API DWORD Impl_GetModuleFileNameW(void* hModule, uint16_t* lpFilename, DWORD nSize)
{
	if (lpFilename == NULL || nSize == 0) return 0;

	char linux_path[1024];
	ssize_t len = readlink("/proc/self/exe", linux_path, sizeof(linux_path) - 1);

	if (len != -1)
	{
		linux_path[len] = '\0';
	}
	else
	{
		// Fallback if readlink fails
		strncpy(linux_path, "/gfl_loader", sizeof(linux_path));
	}

	// We format it as a fake Windows path (e.g. Z:\path\to\loader)
	// Wine uses 'Z:' to map the Linux root directory '/'.
	// Note: We replace '/' with '\' to make Windows path parsers happy.
	char win_path[1024];
	snprintf(win_path, sizeof(win_path), "Z:%s", linux_path);

	for (int i = 0; win_path[i] != '\0'; i++)
	{
		if (win_path[i] == '/')
		{
			win_path[i] = '\\';
		}
	}

	// Convert ASCII to UTF-16
	DWORD chars_copied = 0;
	while (win_path[chars_copied] != '\0' && chars_copied < nSize - 1)
	{
		lpFilename[chars_copied] = (uint16_t)(unsigned char)win_path[chars_copied];
		chars_copied++;
	}
	lpFilename[chars_copied] = 0; // Null terminator

	printf("	[API] Called GetModuleFileNameW -> %s\n", win_path);
	return chars_copied;
}

// Windows API: LPWCH GetEnvironmentStringsW(void)
WIN_API uint16_t *Impl_GetEnvironmentStringsW()
{
	// 1. Calculate total buffer size needed
	size_t total_chars = 0;
	if (environ != NULL)
	{
		for (int i = 0; environ[i] != NULL; i++)
		{
			total_chars += strlen(environ[i]) + 1; // +1 for the null terminator of each string
		}
	}
	total_chars += 1; // +1 for the final double-null terminator

	// 2. Allocate memory (must be freed by FreeEnvironmentStringsW)
	// We use calloc to ensure it's zeroed out (handles the double-null naturally)
	uint16_t *env_block = (uint16_t *)calloc(total_chars, sizeof(uint16_t));
	if (!env_block)
	{
		printf("	[!!!] GetEnvironmentStringsW: Memory allocation failed!\n");
		return NULL;
	}

	// 3. Copy standard Linux environment variables to the Windows buffer
	size_t current_offset = 0;
	if (environ != NULL)
	{
		for (int i = 0; environ[i] != NULL; i++)
		{
			size_t len = strlen(environ[i]);
			AsciiToWchar(environ[i], &env_block[current_offset], len + 1);
			current_offset += len + 1;
		}
	}

	// The block is already zeroed by calloc, so the final '\0' is inherently present.
	printf("	[API] Called GetEnvironmentStringsW -> Allocated Block: %p\n", env_block);
	return env_block;
}

// Windows API: BOOL FreeEnvironmentStringsW(LPWCH penv)
WIN_API DWORD Impl_FreeEnvironmentStringsW(uint16_t *penv)
{
	if (penv)
	{
		free(penv);
	}
	printf("	[API] Called FreeEnvironmentStringsW (%p)\n", penv);
	return 1; // TRUE
}

/**
 * @brief Windows API: int WideCharToMultiByte(...)
 * 
 * @note `WCHAR`: 16bit, `wchar_t`: 32bit
 */
WIN_API int Impl_WideCharToMultiByte(
	DWORD CodePage,
	DWORD dwFlags,
	const uint16_t *lpWideCharStr,
	int cchWideChar,
	char *lpMultiByteStr,
	int cbMultiByte,
	const char *lpDefaultChar,
	DWORD *lpUsedDefaultChar)
{
	// Sanity check
	if (lpWideCharStr == NULL || cchWideChar == 0)
	{
		g_last_error = 87; // ERROR_INVALID_PARAMETER
		return 0;
	}

	// 1. Determine the length of the input string
	int src_len = cchWideChar;
	if (src_len == -1)
	{
		src_len = 0;
		while (lpWideCharStr[src_len] != 0)
		{
			src_len++;
		}
		src_len++; // Include the null terminator in the count
	}

	int bytes_written = 0;
	int required_size = 0;

	// 2. UTF-16 to UTF-8 conversion loop
	for (int i = 0; i < src_len; i++)
	{
		uint32_t wc = lpWideCharStr[i];
		uint32_t codepoint = wc;

		// Handle UTF-16 Surrogate Pairs (Characters outside the BMP, e.g., Emojis)
		if (wc >= 0xD800 && wc <= 0xDBFF && (i + 1) < src_len)
		{
			uint16_t next_wc = lpWideCharStr[i + 1];
			if (next_wc >= 0xDC00 && next_wc <= 0xDFFF)
			{
				codepoint = 0x10000 + ((wc - 0xD800) << 10) + (next_wc - 0xDC00);
				i++; // Skip the low surrogate as we've consumed it
			}
		}

		// Calculate how many bytes this codepoint needs in UTF-8
		int char_bytes = 0;
		if (codepoint <= 0x7F) char_bytes = 1;
		else if (codepoint <= 0x7FF) char_bytes = 2;
		else if (codepoint <= 0xFFFF) char_bytes = 3;
		else char_bytes = 4;

		// If cbMultiByte is 0, the caller just wants to know the required buffer size
		if (cbMultiByte == 0 || lpMultiByteStr == NULL)
		{
			required_size += char_bytes;
		}
		else
		{
			// Check if we have enough space left in the buffer
			if (bytes_written + char_bytes > cbMultiByte)
			{
				g_last_error = 122; // ERROR_INSUFFICIENT_BUFFER
				return 0;
			}

			// Encode and write the UTF-8 bytes
			if (char_bytes == 1)
			{
				lpMultiByteStr[bytes_written++] = (char)codepoint;
			}
			else if (char_bytes == 2)
			{
				lpMultiByteStr[bytes_written++] = (char)(0xC0 | (codepoint >> 6));
				lpMultiByteStr[bytes_written++] = (char)(0x80 | (codepoint & 0x3F));
			}
			else if (char_bytes == 3)
			{
				lpMultiByteStr[bytes_written++] = (char)(0xE0 | (codepoint >> 12));
				lpMultiByteStr[bytes_written++] = (char)(0x80 | ((codepoint >> 6) & 0x3F));
				lpMultiByteStr[bytes_written++] = (char)(0x80 | (codepoint & 0x3F));
			}
			else if (char_bytes == 4)
			{
				lpMultiByteStr[bytes_written++] = (char)(0xF0 | (codepoint >> 18));
				lpMultiByteStr[bytes_written++] = (char)(0x80 | ((codepoint >> 12) & 0x3F));
				lpMultiByteStr[bytes_written++] = (char)(0x80 | ((codepoint >> 6) & 0x3F));
				lpMultiByteStr[bytes_written++] = (char)(0x80 | (codepoint & 0x3F));
			}
		}
	}

	// Return the calculated size if requested
	if (cbMultiByte == 0 || lpMultiByteStr == NULL)
	{
		// Don't print the actual string here to avoid console spam when size-querying
		printf("	[API] WideCharToMultiByte (Query Size) -> %d bytes needed\n", required_size);
		return required_size;
	}

	// Print the first few characters of the converted string for debugging (Environment)
	char debug_str[64] = {0};
	int copy_len = bytes_written < 63 ? bytes_written : 63;
	memcpy(debug_str, lpMultiByteStr, copy_len);

	// Replace newlines with spaces for single-line clean logging
	for(int j = 0; j < copy_len; j++)
	{
		if(debug_str[j] == '\n' || debug_str[j] == '\r') debug_str[j] = ' ';
	}

	printf("	[API] WideCharToMultiByte (Written: %d bytes) -> \"%s...\"\n", bytes_written, debug_str);
	return bytes_written;
}

// =========================================================================
// Virtual Memory Allocation Tracker (For VirtualFree dwSize = 0 support)
// =========================================================================

#define MAX_VIRTUAL_ALLOCS 4096
typedef struct
{
	void* addr;
	size_t size;
} VIRTUAL_ALLOC_RECORD;

static VIRTUAL_ALLOC_RECORD g_valloc_tracker[MAX_VIRTUAL_ALLOCS];
static pthread_mutex_t g_valloc_mutex = PTHREAD_MUTEX_INITIALIZER;

static void TrackVirtualAlloc(void* addr, size_t size)
{
	pthread_mutex_lock(&g_valloc_mutex);
	for (int i = 0; i < MAX_VIRTUAL_ALLOCS; i++)
	{
		if (g_valloc_tracker[i].addr == NULL)
		{
			g_valloc_tracker[i].addr = addr;
			g_valloc_tracker[i].size = size;
			break;
		}
	}
	pthread_mutex_unlock(&g_valloc_mutex);
}

static size_t UntrackVirtualAlloc(void* addr)
{
	size_t size = 0;
	pthread_mutex_lock(&g_valloc_mutex);
	for (int i = 0; i < MAX_VIRTUAL_ALLOCS; i++)
	{
		if (g_valloc_tracker[i].addr == addr)
		{
			size = g_valloc_tracker[i].size;
			g_valloc_tracker[i].addr = NULL;
			g_valloc_tracker[i].size = 0;
			break;
		}
	}
	pthread_mutex_unlock(&g_valloc_mutex);
	return size;
}

static int WinProtToLinuxProt(DWORD flProtect)
{
	if (flProtect & PAGE_EXECUTE_READWRITE)
		return PROT_READ | PROT_WRITE | PROT_EXEC;
	if (flProtect & PAGE_EXECUTE_READ)
		return PROT_READ | PROT_EXEC;
	if (flProtect & PAGE_EXECUTE)
		return PROT_EXEC;
	if (flProtect & PAGE_READWRITE)
		return PROT_READ | PROT_WRITE;
	if (flProtect & PAGE_READONLY)
		return PROT_READ;
	if (flProtect == PAGE_NOACCESS)
		return PROT_NONE;
	return PROT_READ | PROT_WRITE; // Fallback
}

// =========================================================================
// Virtual Memory API Implementations
// =========================================================================

// Windows API: LPVOID VirtualAlloc(LPVOID lpAddress, SIZE_T dwSize, DWORD flAllocationType, DWORD flProtect)
WIN_API void* Impl_VirtualAlloc(void* lpAddress, size_t dwSize, DWORD flAllocationType, DWORD flProtect)
{
	int linux_prot = WinProtToLinuxProt(flProtect);
	int linux_flags = MAP_PRIVATE | MAP_ANONYMOUS;

	// Handle memory allocation
	// If lpAddress is provided, Windows attempts to allocate at that specific address.
	// In mmap, providing an address is just a "hint" unless MAP_FIXED is used.
	// We avoid MAP_FIXED because it overwrites existing memory if it overlaps, which is dangerous.
	void* ptr = mmap(lpAddress, dwSize, linux_prot, linux_flags, -1, 0);

	if (ptr == MAP_FAILED) {
		g_last_error = 8; // ERROR_NOT_ENOUGH_MEMORY
		printf("	[API] VirtualAlloc (Size: %zu, Prot: 0x%X) -> FAILED!\n", dwSize, (unsigned int)flProtect);
		return NULL;
	}

	// Record size for VirtualFree
	TrackVirtualAlloc(ptr, dwSize);

	printf("	[API] VirtualAlloc (Size: %zu, Prot: 0x%X) -> %p\n", dwSize, (unsigned int)flProtect, ptr);
	return ptr;
}

// Windows API: BOOL VirtualFree(LPVOID lpAddress, SIZE_T dwSize, DWORD dwFreeType)
WIN_API DWORD Impl_VirtualFree(void* lpAddress, size_t dwSize, DWORD dwFreeType)
{
	if (dwFreeType & MEM_RELEASE) {
		// Windows documentation: If MEM_RELEASE, dwSize MUST be 0.
		size_t tracked_size = UntrackVirtualAlloc(lpAddress);
		if (tracked_size > 0) {
			munmap(lpAddress, tracked_size);
			printf("	[API] VirtualFree (Release, Addr: %p, Tracked Size: %zu) -> OK\n", lpAddress, tracked_size);
			return 1;
		}
		printf("	[API] VirtualFree (Release, Addr: %p) -> FAILED (Not Tracked)\n", lpAddress);
		return 0;
	} 
	else if (dwFreeType & MEM_DECOMMIT) {
		// Simulate Decommit by removing all access
		mprotect(lpAddress, dwSize, PROT_NONE);
		printf("	[API] VirtualFree (Decommit, Addr: %p, Size: %zu) -> OK\n", lpAddress, dwSize);
		return 1;
	}
	
	return 0;
}

// Windows API: BOOL VirtualProtect(LPVOID lpAddress, SIZE_T dwSize, DWORD flNewProtect, PDWORD lpflOldProtect)
WIN_API DWORD Impl_VirtualProtect(void* lpAddress, size_t dwSize, DWORD flNewProtect, DWORD* lpflOldProtect)
{
	// Provide a dummy old protect value to keep the caller happy
	if (lpflOldProtect) {
		*lpflOldProtect = PAGE_READWRITE;
	}

	// mprotect requires the address to be exactly page-aligned
	uintptr_t page_mask = ~((uintptr_t)sysconf(_SC_PAGESIZE) - 1);
	void* aligned_addr = (void*)((uintptr_t)lpAddress & page_mask);
	
	// Stretch the size to cover the difference caused by aligning the address down
	size_t aligned_size = dwSize + ((uintptr_t)lpAddress - (uintptr_t)aligned_addr);

	int linux_prot = WinProtToLinuxProt(flNewProtect);
	int ret = mprotect(aligned_addr, aligned_size, linux_prot);

	printf("	[API] VirtualProtect (%p, Size: %zu, NewProt: 0x%X) -> %s\n", lpAddress, dwSize, flNewProtect, ret == 0 ? "OK" : "FAILED");
	
	return ret == 0 ? 1 : 0;
}

// Windows API: SIZE_T VirtualQuery(LPCVOID lpAddress, PMEMORY_BASIC_INFORMATION lpBuffer, SIZE_T dwLength)
WIN_API size_t Impl_VirtualQuery(void* lpAddress, MEMORY_BASIC_INFORMATION* lpBuffer, size_t dwLength)
{
	if (dwLength < sizeof(MEMORY_BASIC_INFORMATION)) return 0;
	
	memset(lpBuffer, 0, sizeof(MEMORY_BASIC_INFORMATION));
	
	// Default to FREE if we don't find it
	lpBuffer->State = MEM_FREE;
	lpBuffer->Protect = PAGE_NOACCESS;
	lpBuffer->BaseAddress = (void*)((uintptr_t)lpAddress & ~0xFFFULL);
	lpBuffer->RegionSize = 0x1000;

	// Parse Linux /proc/self/maps to give highly accurate memory state to Unity's GC
	FILE* fp = fopen("/proc/self/maps", "r");
	if (fp) {
		char line[512];
		while (fgets(line, sizeof(line), fp)) {
			uintptr_t start, end;
			char perms[8];
			
			if (sscanf(line, "%lx-%lx %7s", &start, &end, perms) == 3) {
				if ((uintptr_t)lpAddress >= start && (uintptr_t)lpAddress < end) {
					lpBuffer->BaseAddress = (void*)start;
					lpBuffer->AllocationBase = (void*)start;
					lpBuffer->RegionSize = end - start;
					lpBuffer->State = MEM_COMMIT;
					lpBuffer->Type = MEM_PRIVATE;

					DWORD prot = 0;
					if (perms[0] == 'r') prot |= PAGE_READONLY;
					if (perms[1] == 'w') prot = PAGE_READWRITE; // override
					
					if (perms[2] == 'x') {
						if (prot == PAGE_READWRITE) prot = PAGE_EXECUTE_READWRITE;
						else if (prot == PAGE_READONLY) prot = PAGE_EXECUTE_READ;
						else prot = PAGE_EXECUTE;
					}
					
					if (prot == 0) prot = PAGE_NOACCESS;
					
					lpBuffer->Protect = prot;
					lpBuffer->AllocationProtect = prot;
					break;
				}
			}
		}
		fclose(fp);
	}

	// Prevent console spam as this is called thousands of times per second by the GC
	// printf("	[API] VirtualQuery (%p) -> State: 0x%X\n", lpAddress, lpBuffer->State);

	return sizeof(MEMORY_BASIC_INFORMATION);
}

// Windows API: BOOL InitializeCriticalSectionAndSpinCount(LPCRITICAL_SECTION lpCriticalSection, DWORD dwSpinCount)
WIN_API DWORD Impl_InitializeCriticalSectionAndSpinCount(void *lpCriticalSection, DWORD dwSpinCount)
{
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);

    // CRITICAL REQUIREMENT: Windows critical sections are recursive by default.
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);

    // We cast the 40-byte Windows struct directly to a 40-byte Linux pthread_mutex_t
    pthread_mutex_init((pthread_mutex_t *)lpCriticalSection, &attr);
    pthread_mutexattr_destroy(&attr);

    // Print the action. (This might be called frequently during init, 
    // but usually stabilizes during gameplay).
    printf("    [API] InitializeCriticalSectionAndSpinCount (%p, SpinCount: %u)\n", lpCriticalSection, dwSpinCount);
    
    return 1; // TRUE
}

/**
 * ====================================================================================================
 * @section III: Hook Table for Loader
 * ====================================================================================================
 */

// clang-format off
const REAL_API_ENTRY g_real_api_hooks[] = {
	{"GetSystemTimeAsFileTime", (void *)Impl_GetSystemTimeAsFileTime},
	{"GetCurrentThreadId", (void *)Impl_GetCurrentThreadId},
	{"GetCurrentProcessId", (void *)Impl_GetCurrentProcessId},
	{"QueryPerformanceCounter", (void *)Impl_QueryPerformanceCounter},
	{"QueryPerformanceFrequency", (void *)Impl_QueryPerformanceFrequency},
	{"LoadLibraryExW", (void *)Impl_LoadLibraryExW},
	{"GetProcAddress", (void *)Impl_GetProcAddress},
	{"InitializeCriticalSectionEx", (void *)Impl_InitializeCriticalSectionEx},
	{"EnterCriticalSection", (void *)Impl_EnterCriticalSection},
	{"LeaveCriticalSection", (void *)Impl_LeaveCriticalSection},
	{"DeleteCriticalSection", (void *)Impl_DeleteCriticalSection},
	{"FreeLibrary", (void *)Impl_FreeLibrary},
	{"TlsAlloc", (void *)Impl_TlsAlloc},
	{"TlsGetValue", (void *)Impl_TlsGetValue},
	{"TlsSetValue", (void *)Impl_TlsSetValue},
	{"TlsFree", (void *)Impl_TlsFree},
	{"GetProcessHeap", (void *)Impl_GetProcessHeap},
	{"GetLastError", (void *)Impl_GetLastError},
	{"SetLastError", (void *)Impl_SetLastError},
	{"HeapAlloc", (void *)Impl_HeapAlloc},
	{"HeapFree", (void *)Impl_HeapFree},
	{"GetStartupInfoW", (void *)Impl_GetStartupInfoW},
	{"GetStdHandle", (void *)Impl_GetStdHandle},
	{"GetFileType", (void *)Impl_GetFileType},
	{"GetCommandLineA", (void *)Impl_GetCommandLineA},
	{"GetCommandLineW", (void *)Impl_GetCommandLineW},
	{"GetACP", (void*)Impl_GetACP},
	{"GetOEMCP", (void*)Impl_GetOEMCP},
	{"IsValidCodePage", (void*)Impl_IsValidCodePage},
	{"InitializeSListHead", (void *)Impl_InitializeSListHead},
	{"GetModuleHandleW", (void *)Impl_GetModuleHandleW},
	{"IsProcessorFeaturePresent", (void*)Impl_IsProcessorFeaturePresent},
	{"CreateEventW", (void*)Impl_CreateEventW},
	{"SetEvent", (void*)Impl_SetEvent},
	{"ResetEvent", (void*)Impl_ResetEvent},
	{"CloseHandle", (void*)Impl_CloseHandle},
	{"GetModuleFileNameW", (void*)Impl_GetModuleFileNameW},
	{"GetEnvironmentStringsW", (void*)Impl_GetEnvironmentStringsW},
	{"FreeEnvironmentStringsW", (void*)Impl_FreeEnvironmentStringsW},
	{"WideCharToMultiByte", (void*)Impl_WideCharToMultiByte},
	{"VirtualAlloc", (void*)Impl_VirtualAlloc},
	{"VirtualFree", (void*)Impl_VirtualFree},
	{"VirtualProtect", (void*)Impl_VirtualProtect},
	{"VirtualQuery", (void*)Impl_VirtualQuery},
	{"InitializeCriticalSectionAndSpinCount", (void*)Impl_InitializeCriticalSectionAndSpinCount},
	{0, 0} // Terminator
};
// clang-format on