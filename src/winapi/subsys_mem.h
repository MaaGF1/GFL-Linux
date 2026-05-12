#pragma once

#include "winapi_core.h"

WIN_API void *Impl_VirtualAlloc(void* lpAddress, size_t dwSize, DWORD flAllocationType, DWORD flProtect);
WIN_API DWORD Impl_VirtualFree(void* lpAddress, size_t dwSize, DWORD dwFreeType);
WIN_API DWORD Impl_VirtualProtect(void* lpAddress, size_t dwSize, DWORD flNewProtect, DWORD* lpflOldProtect);
WIN_API size_t Impl_VirtualQuery(void* lpAddress, MEMORY_BASIC_INFORMATION* lpBuffer, size_t dwLength);

WIN_API void *Impl_GetProcessHeap();
WIN_API void *Impl_HeapAlloc(void *hHeap, DWORD dwFlags, size_t dwBytes);
WIN_API DWORD Impl_HeapFree(void *hHeap, DWORD dwFlags, void *lpMem);