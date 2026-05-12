#include "subsys_sync.h"

// Windows API: BOOL InitializeCriticalSectionEx(LPCRITICAL_SECTION lpCriticalSection, DWORD dwSpinCount, DWORD Flags)
WIN_API DWORD Impl_InitializeCriticalSectionEx(void *lpCriticalSection, DWORD dwSpinCount, DWORD Flags)
{
	pthread_mutexattr_t attr;
	pthread_mutexattr_init(&attr);

	// CRITICAL REQUIREMENT: Windows critical sections are recursive by default.
	pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);

	// We cast the 40-byte Windows struct directly to a 40-byte Linux pthread_mutex_t
	pthread_mutex_init((pthread_mutex_t *)lpCriticalSection, &attr);
	pthread_mutexattr_destroy(&attr);

	printf("	[API] InitializeCriticalSectionEx (%p)\n", lpCriticalSection);
	return 1; // TRUE
}

// Windows API: BOOL InitializeCriticalSectionAndSpinCount(LPCRITICAL_SECTION lpCriticalSection, DWORD dwSpinCount)
WIN_API DWORD Impl_InitializeCriticalSectionAndSpinCount(void *lpCriticalSection, DWORD dwSpinCount)
{
	pthread_mutexattr_t attr;
	pthread_mutexattr_init(&attr);

	// CRITICAL REQUIREMENT: Windows critical sections are recursive by default.
	pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);

	// We cast the 40-byte Windows struct directly to a 40-byte Linux pthread_mutex_t
	pthread_mutex_init((pthread_mutex_t *)lpCriticalSection, &attr);
	pthread_mutexattr_destroy(&attr);

	// Print the action. (This might be called frequently during init,
	// but usually stabilizes during gameplay).
	printf("	[API] InitializeCriticalSectionAndSpinCount (%p, SpinCount: %u)\n", lpCriticalSection, dwSpinCount);

	return 1; // TRUE
}

// Windows API: void EnterCriticalSection(LPCRITICAL_SECTION lpCriticalSection)
WIN_API void Impl_EnterCriticalSection(void *lpCriticalSection)
{
	pthread_mutex_lock((pthread_mutex_t *)lpCriticalSection);
	// Note: We don't print here to avoid console spam, as this is called millions of times per second in a game.
}

// Windows API: void LeaveCriticalSection(LPCRITICAL_SECTION lpCriticalSection)
WIN_API void Impl_LeaveCriticalSection(void *lpCriticalSection)
{
	pthread_mutex_unlock((pthread_mutex_t *)lpCriticalSection);
}

// Windows API: void DeleteCriticalSection(LPCRITICAL_SECTION lpCriticalSection)
WIN_API void Impl_DeleteCriticalSection(void *lpCriticalSection)
{
	pthread_mutex_destroy((pthread_mutex_t *)lpCriticalSection);
	printf("	[API] DeleteCriticalSection (%p)\n", lpCriticalSection);
}

// Windows API: HANDLE CreateEventW(LPSECURITY_ATTRIBUTES lpEventAttributes, BOOL bManualReset, BOOL bInitialState, LPCWSTR lpName)
WIN_API void *Impl_CreateEventW(void *lpEventAttributes, DWORD bManualReset, DWORD bInitialState, const uint16_t *lpName)
{
	// Allocate our custom event structure
	WIN_EVENT *ev = (WIN_EVENT *)malloc(sizeof(WIN_EVENT));
	if (!ev)
		return NULL;

	ev->magic = EVENT_MAGIC;
	ev->manual_reset = bManualReset;
	ev->is_signaled = bInitialState;

	pthread_mutex_init(&ev->mutex, NULL);
	pthread_cond_init(&ev->cond, NULL);

	// Optional: Extract name for debugging if lpName is not NULL
	char name_buf[128] = "Unnamed";
	if (lpName)
	{
		WcharToAscii(lpName, name_buf, sizeof(name_buf));
	}

	printf("	[API] CreateEventW (Name: %s, Manual: %u, Signaled: %u) -> Handle: %p\n", name_buf, bManualReset, bInitialState, ev);

	return ev;
}

// Windows API: BOOL SetEvent(HANDLE hEvent)
WIN_API DWORD Impl_SetEvent(void *hEvent)
{
	if (!hEvent)
		return 0;
	WIN_EVENT *ev = (WIN_EVENT *)hEvent;

	if (ev->magic == EVENT_MAGIC)
	{
		pthread_mutex_lock(&ev->mutex);
		ev->is_signaled = 1;

		if (ev->manual_reset)
		{
			// Wake ALL waiting threads
			pthread_cond_broadcast(&ev->cond);
		}
		else
		{
			// Wake ONLY ONE waiting thread (Auto-reset)
			pthread_cond_signal(&ev->cond);
		}
		pthread_mutex_unlock(&ev->mutex);

		// Note: SetEvent is called extremely often, better to keep printing disabled or minimal
		// printf("	[API] SetEvent (%p)\n", hEvent);
		return 1; // TRUE
	}
	return 0; // FALSE
}

// Windows API: BOOL ResetEvent(HANDLE hEvent)
WIN_API DWORD Impl_ResetEvent(void *hEvent)
{
	if (!hEvent)
		return 0;
	WIN_EVENT *ev = (WIN_EVENT *)hEvent;

	if (ev->magic == EVENT_MAGIC)
	{
		pthread_mutex_lock(&ev->mutex);
		ev->is_signaled = 0;
		pthread_mutex_unlock(&ev->mutex);
		return 1; // TRUE
	}
	return 0; // FALSE
}

// Windows API: void InitializeSListHead(PSLIST_HEADER ListHead)
WIN_API void Impl_InitializeSListHead(PSLIST_HEADER ListHead)
{
	if (ListHead)
	{
		memset(ListHead, 0, sizeof(SLIST_HEADER));
	}
	printf("	[API] InitializeSListHead (%p)\n", ListHead);
}