#include "subsys_sys.h"
#include "winapi_registry.h" // Needed for FindThunkByName inside GetProcAddress

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