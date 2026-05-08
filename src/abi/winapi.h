#pragma once

/**
 * @file src/abi/winapi.h
 * @author @SwordofMorning
 * @brief Implementations of Windows APIs on Linux
 * @version 0.1
 * @date 2026-05-08
 * 
 * @copyright Copyright (c) 2026
 * 
 */

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

#include "../include/type.h"
#include "../include/windows.h"

typedef struct
{
	const char *name;
	void *thunk_ptr;
} REAL_API_ENTRY;

extern const REAL_API_ENTRY g_real_api_hooks[];

void *FindRealThunkByName(const char *name);
void *FindThunkByName(const char *name);

#define WIN_API extern "C" __attribute__((ms_abi, force_align_arg_pointer))