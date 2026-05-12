#pragma once
#include "winapi_core.h"

WIN_API int    Impl_WideCharToMultiByte(DWORD CodePage, DWORD dwFlags, const uint16_t *lpWideCharStr, int cchWideChar, char *lpMultiByteStr, int cbMultiByte, const char *lpDefaultChar, DWORD *lpUsedDefaultChar);
WIN_API DWORD  Impl_GetACP();
WIN_API DWORD  Impl_GetOEMCP();
WIN_API DWORD  Impl_IsValidCodePage(DWORD CodePage);