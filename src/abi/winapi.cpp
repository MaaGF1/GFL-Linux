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
#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/syscall.h>

typedef uint32_t DWORD;
typedef uint64_t QWORD;

/**
 * =========================================================================
 * 
 * @par C++ Implementations 
 * 
 * @section Windows x64 ABI
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
 * @section System V AMD64 ABI
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
 * =========================================================================
 */

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

	printf("	[API] Called GetSystemTimeAsFileTime (Unix TS: %lu)\n", ts.tv_sec);
}

// Windows API: DWORD GetCurrentThreadId(void)
extern "C" DWORD Impl_GetCurrentThreadId()
{
	pid_t tid = syscall(SYS_gettid);
	printf("	[API] Called GetCurrentThreadId (TID: %u)\n", (unsigned int)tid);
	return (DWORD)tid;
}

// Windows API: DWORD GetCurrentProcessId(void)
extern "C" DWORD Impl_GetCurrentProcessId()
{
	// getpid() is a standard POSIX function provided by unistd.h
	pid_t pid = getpid();
	printf("	[API] Called GetCurrentProcessId (PID: %u)\n", (unsigned int)pid);
	return (DWORD)pid;
}

// =========================================================================
// 2. Assembly Thunks (Win64 ABI -> SysV ABI)
// =========================================================================

__asm__(
	".text\n"
	".global Thunk_Real_GetSystemTimeAsFileTime\n"
	"Thunk_Real_GetSystemTimeAsFileTime:\n"
	"	mov %rcx, %rdi\n" // Win64 Arg 1 (RCX) -> SysV Arg 1 (RDI)
	"	jmp Impl_GetSystemTimeAsFileTime\n"
);
extern "C" void Thunk_Real_GetSystemTimeAsFileTime();

__asm__(
	".global Thunk_Real_GetCurrentThreadId\n"
	"Thunk_Real_GetCurrentThreadId:\n"
	"	jmp Impl_GetCurrentThreadId\n" // No params to pass, just call it
);
extern "C" void Thunk_Real_GetCurrentThreadId();

__asm__(
    ".global Thunk_Real_GetCurrentProcessId\n"
    "Thunk_Real_GetCurrentProcessId:\n"
    "    jmp Impl_GetCurrentProcessId\n" // No params to pass, just call it
);
extern "C" void Thunk_Real_GetCurrentProcessId();

// =========================================================================
// 3. Hook Table for Loader
// =========================================================================

const REAL_API_ENTRY g_real_api_hooks[] = {
	{"GetSystemTimeAsFileTime", (void*)Thunk_Real_GetSystemTimeAsFileTime},
	{"GetCurrentThreadId", (void*)Thunk_Real_GetCurrentThreadId},
	{"GetCurrentProcessId", (void*)Thunk_Real_GetCurrentProcessId},
	{0, 0} // Terminator
};