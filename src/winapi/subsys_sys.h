#pragma once
#include "winapi_core.h"

WIN_API void Impl_GetSystemTimeAsFileTime(DWORD *lpSystemTimeAsFileTime);
WIN_API DWORD Impl_GetCurrentThreadId();
WIN_API DWORD Impl_GetCurrentProcessId();
WIN_API DWORD Impl_QueryPerformanceCounter(QWORD *lpPerformanceCount);
WIN_API DWORD Impl_QueryPerformanceFrequency(QWORD *lpFrequency);

WIN_API void *Impl_LoadLibraryExW(const uint16_t *lpLibFileName, void *hFile, DWORD dwFlags);
WIN_API DWORD Impl_FreeLibrary(void *hLibModule);
WIN_API void *Impl_GetProcAddress(void *hModule, const char *lpProcName);
WIN_API void *Impl_GetModuleHandleW(const uint16_t *lpModuleName);
WIN_API DWORD Impl_GetModuleFileNameW(void *hModule, uint16_t *lpFilename, DWORD nSize);

WIN_API void Impl_GetStartupInfoW(STARTUPINFOW *lpStartupInfo);
WIN_API DWORD Impl_GetLastError();
WIN_API void Impl_SetLastError(DWORD dwErrCode);
WIN_API DWORD Impl_IsProcessorFeaturePresent(DWORD ProcessorFeature);

WIN_API uint16_t *Impl_GetEnvironmentStringsW();
WIN_API DWORD  Impl_FreeEnvironmentStringsW(uint16_t *penv);

WIN_API DWORD Impl_TlsAlloc();
WIN_API void *Impl_TlsGetValue(DWORD dwTlsIndex);
WIN_API DWORD Impl_TlsSetValue(DWORD dwTlsIndex, void *lpTlsValue);
WIN_API DWORD Impl_TlsFree(DWORD dwTlsIndex);