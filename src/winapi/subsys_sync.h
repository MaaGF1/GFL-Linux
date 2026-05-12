#pragma once
#include "winapi_core.h"

WIN_API DWORD Impl_InitializeCriticalSectionEx(void *lpCriticalSection, DWORD dwSpinCount, DWORD Flags);
WIN_API DWORD Impl_InitializeCriticalSectionAndSpinCount(void *lpCriticalSection, DWORD dwSpinCount);
WIN_API void Impl_EnterCriticalSection(void *lpCriticalSection);
WIN_API void Impl_LeaveCriticalSection(void *lpCriticalSection);
WIN_API void Impl_DeleteCriticalSection(void *lpCriticalSection);

WIN_API void *Impl_CreateEventW(void *lpEventAttributes, DWORD bManualReset, DWORD bInitialState, const uint16_t *lpName);
WIN_API DWORD Impl_SetEvent(void *hEvent);
WIN_API DWORD Impl_ResetEvent(void *hEvent);

WIN_API void Impl_InitializeSListHead(PSLIST_HEADER ListHead);