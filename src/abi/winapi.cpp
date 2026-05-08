/**
 * @file winapi.cpp
 * @brief Implementations of Windows APIs on Linux
 */
#include "winapi.h"
#include <stdio.h>
#include <time.h>
#include <sys/time.h>

typedef uint32_t DWORD;
typedef uint64_t QWORD;

// =========================================================================
// 1. C++ Implementations (SysV ABI: args in RDI, RSI, RDX...)
// =========================================================================

// Windows API: void GetSystemTimeAsFileTime(LPFILETIME lpSystemTimeAsFileTime)
extern "C" void Impl_GetSystemTimeAsFileTime(DWORD* lpSystemTimeAsFileTime)
{
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);

    // 1601 to 1970 is 11644473600 seconds
    QWORD filetime = ((QWORD)ts.tv_sec + 11644473600ULL) * 10000000ULL;
    filetime += (QWORD)ts.tv_nsec / 100ULL;

    // FILETIME is a struct of two DWORDs (Low, High)
    lpSystemTimeAsFileTime[0] = (DWORD)(filetime & 0xFFFFFFFF);
    lpSystemTimeAsFileTime[1] = (DWORD)(filetime >> 32);

    printf("    [API] Called GetSystemTimeAsFileTime (Unix TS: %lu)\n", ts.tv_sec);
}


// =========================================================================
// 2. Assembly Thunks (Win64 ABI -> SysV ABI)
// =========================================================================

__asm__(
    ".text\n"
    ".global Thunk_Real_GetSystemTimeAsFileTime\n"
    "Thunk_Real_GetSystemTimeAsFileTime:\n"
    "    mov %rcx, %rdi\n" // Win64 Arg 1 (RCX) -> SysV Arg 1 (RDI)
    "    jmp Impl_GetSystemTimeAsFileTime\n"
);
extern "C" void Thunk_Real_GetSystemTimeAsFileTime();


// =========================================================================
// 3. Hook Table for Loader
// =========================================================================

const REAL_API_ENTRY g_real_api_hooks[] = {
    {"GetSystemTimeAsFileTime", (void*)Thunk_Real_GetSystemTimeAsFileTime},
    // Future implemented APIs will be added here...
    {0, 0} // Terminator
};