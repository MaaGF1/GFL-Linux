#pragma once

#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <pthread.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/time.h>
#include <sys/syscall.h>
#include <sys/mman.h>

#include "../include/type.h"
#include "../include/windows.h"

/**
 * ====================================================================================================
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

// Windows API Calling Convention Macro
#define WIN_API extern "C" __attribute__((ms_abi, force_align_arg_pointer))

// Global States
extern thread_local DWORD g_last_error;
// Provided by main.cpp
extern BYTE* g_mapped_base;
// Provided by Linux libc
extern char** environ;

// Core Utility Functions
void WcharToAscii(const uint16_t *wstr, char *out_buf, size_t max_len);
void AsciiToWchar(const char *str, uint16_t *wstr, size_t max_chars);