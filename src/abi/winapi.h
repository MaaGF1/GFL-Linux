/**
 * @file winapi.h
 * @brief Declarations for implemented Windows APIs
 */
#ifndef WINAPI_H
#define WINAPI_H

#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/syscall.h>

typedef struct {
    const char *name;
    void *thunk_ptr;
} REAL_API_ENTRY;

extern const REAL_API_ENTRY g_real_api_hooks[];

void* FindRealThunkByName(const char* name);
void* FindThunkByName(const char* name);

#endif // WINAPI_H