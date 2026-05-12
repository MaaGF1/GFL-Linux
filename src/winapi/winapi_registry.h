#pragma once

#include "winapi_core.h"

typedef struct
{
	const char *name;
	void *thunk_ptr;
} REAL_API_ENTRY;

extern const REAL_API_ENTRY g_real_api_hooks[];

// This assumes auto_iat_hooks exists globally
void *FindRealThunkByName(const char *name);
void *FindThunkByName(const char *name);