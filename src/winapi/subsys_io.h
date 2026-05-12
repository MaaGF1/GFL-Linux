#pragma once
#include "winapi_core.h"

WIN_API void *Impl_GetStdHandle(DWORD nStdHandle);
WIN_API DWORD Impl_GetFileType(void *hFile);
WIN_API char *Impl_GetCommandLineA();
WIN_API uint16_t *Impl_GetCommandLineW();
WIN_API DWORD Impl_CloseHandle(void *hObject);