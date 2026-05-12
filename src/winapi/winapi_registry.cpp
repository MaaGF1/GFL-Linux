#include "winapi_registry.h"

// Include all subsystems
#include "subsys_mem.h"
#include "subsys_sync.h"
#include "subsys_sys.h"
#include "subsys_string.h"
#include "subsys_io.h"

// clang-format off
const REAL_API_ENTRY g_real_api_hooks[] = {
	// --- Memory Subsystem ---
	{"GetProcessHeap", (void *)Impl_GetProcessHeap},
	{"HeapAlloc", (void *)Impl_HeapAlloc},
	{"HeapFree", (void *)Impl_HeapFree},
	{"VirtualAlloc", (void*)Impl_VirtualAlloc},
	{"VirtualFree", (void*)Impl_VirtualFree},
	{"VirtualProtect", (void*)Impl_VirtualProtect},
	{"VirtualQuery", (void*)Impl_VirtualQuery},

	// --- Sync Subsystem ---
	{"InitializeCriticalSectionEx", (void *)Impl_InitializeCriticalSectionEx},
	{"InitializeCriticalSectionAndSpinCount", (void*)Impl_InitializeCriticalSectionAndSpinCount},
	{"EnterCriticalSection", (void *)Impl_EnterCriticalSection},
	{"LeaveCriticalSection", (void *)Impl_LeaveCriticalSection},
	{"DeleteCriticalSection", (void *)Impl_DeleteCriticalSection},
	{"CreateEventW", (void*)Impl_CreateEventW},
	{"SetEvent", (void*)Impl_SetEvent},
	{"ResetEvent", (void*)Impl_ResetEvent},
	{"InitializeSListHead", (void *)Impl_InitializeSListHead},

	// --- System & Process Subsystem ---
	{"GetSystemTimeAsFileTime", (void *)Impl_GetSystemTimeAsFileTime},
	{"GetCurrentThreadId", (void *)Impl_GetCurrentThreadId},
	{"GetCurrentProcessId", (void *)Impl_GetCurrentProcessId},
	{"QueryPerformanceCounter", (void *)Impl_QueryPerformanceCounter},
	{"QueryPerformanceFrequency", (void *)Impl_QueryPerformanceFrequency},
	{"LoadLibraryExW", (void *)Impl_LoadLibraryExW},
	{"FreeLibrary", (void *)Impl_FreeLibrary},
	{"GetProcAddress", (void *)Impl_GetProcAddress},
	{"GetModuleHandleW", (void *)Impl_GetModuleHandleW},
	{"GetModuleFileNameW", (void*)Impl_GetModuleFileNameW},
	{"GetStartupInfoW", (void *)Impl_GetStartupInfoW},
	{"GetLastError", (void *)Impl_GetLastError},
	{"SetLastError", (void *)Impl_SetLastError},
	{"IsProcessorFeaturePresent", (void*)Impl_IsProcessorFeaturePresent},
	{"GetEnvironmentStringsW", (void*)Impl_GetEnvironmentStringsW},
	{"FreeEnvironmentStringsW", (void*)Impl_FreeEnvironmentStringsW},
	{"TlsAlloc", (void *)Impl_TlsAlloc},
	{"TlsGetValue", (void *)Impl_TlsGetValue},
	{"TlsSetValue", (void *)Impl_TlsSetValue},
	{"TlsFree", (void *)Impl_TlsFree},

	// --- String Subsystem ---
	{"WideCharToMultiByte", (void*)Impl_WideCharToMultiByte},
	{"GetACP", (void*)Impl_GetACP},
	{"GetOEMCP", (void*)Impl_GetOEMCP},
	{"IsValidCodePage", (void*)Impl_IsValidCodePage},

	// --- I/O Subsystem ---
	{"GetStdHandle", (void *)Impl_GetStdHandle},
	{"GetFileType", (void *)Impl_GetFileType},
	{"GetCommandLineA", (void *)Impl_GetCommandLineA},
	{"GetCommandLineW", (void *)Impl_GetCommandLineW},
	{"CloseHandle", (void*)Impl_CloseHandle},

	{0, 0} // Terminator
};
// clang-format on