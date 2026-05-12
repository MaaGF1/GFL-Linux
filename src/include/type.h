#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef uint8_t BYTE;
typedef uint16_t WORD;
typedef uint32_t DWORD;
typedef uint64_t QWORD;

#define CP_UTF8 65001

// =========================================================================
// Emulated Windows Event Structure
// =========================================================================

// 'EVNT'
#define EVENT_MAGIC 0x45564E54

typedef struct
{
	// Magic number to identify our event handles
	DWORD magic;
	pthread_mutex_t mutex;
	pthread_cond_t cond;
	// BOOL: Does it stay signaled until manually reset?
	DWORD manual_reset;
	// BOOL: Current state
	DWORD is_signaled;
} WIN_EVENT;

#ifdef __cplusplus
}
#endif