/**
 * @file winapi.h
 * @brief Declarations for implemented Windows APIs
 */
#ifndef WINAPI_H
#define WINAPI_H

#include <stdint.h>

typedef struct {
    const char *name;
    void *thunk_ptr;
} REAL_API_ENTRY;

extern const REAL_API_ENTRY g_real_api_hooks[];

#endif // WINAPI_H