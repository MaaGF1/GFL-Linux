#include "subsys_io.h"

// Windows API: HANDLE GetStdHandle(DWORD nStdHandle)
WIN_API void *Impl_GetStdHandle(DWORD nStdHandle)
{
	void *result = NULL;

	// clang-format off
	switch (nStdHandle)
	{
		case STD_INPUT_HANDLE:
			result = (void *)((uintptr_t)STD_HANDLE_BASE + 0);
			printf("	[API] Called GetStdHandle (STD_INPUT_HANDLE) -> %p\n", result);
			break;
		case STD_OUTPUT_HANDLE:
			result = (void *)((uintptr_t)STD_HANDLE_BASE + 1);
			printf("	[API] Called GetStdHandle (STD_OUTPUT_HANDLE) -> %p\n", result);
			break;
		case STD_ERROR_HANDLE:
			result = (void *)((uintptr_t)STD_HANDLE_BASE + 2);
			printf("	[API] Called GetStdHandle (STD_ERROR_HANDLE) -> %p\n", result);
			break;
		default:
			printf("	[!!!] GetStdHandle called with unknown handle type: %u\n", (unsigned int)nStdHandle);
			g_last_error = 87; // ERROR_INVALID_PARAMETER
			result = (void *)(uintptr_t)-1; // INVALID_HANDLE_VALUE
			break;
	}
	// clang-format on

	return result;
}

// Windows API: DWORD GetFileType(HANDLE hFile)
WIN_API DWORD Impl_GetFileType(void *hFile)
{
	uintptr_t handle_val = (uintptr_t)hFile;
	DWORD result = FILE_TYPE_UNKNOWN;

	// Check if it's one of our standard handles
	if (handle_val >= (uintptr_t)STD_HANDLE_BASE && handle_val <= (uintptr_t)STD_HANDLE_BASE + 2)
	{
		// Standard I/O handles are character devices (terminal)
		result = FILE_TYPE_CHAR;
	}

	printf("	[API] Called GetFileType (Handle: %p) -> Type: %u\n", hFile, result);
	return result;
}

// Windows API: LPSTR GetCommandLineA(void)
WIN_API char *Impl_GetCommandLineA()
{
	static char cmdline[4096] = {0};
	static bool initialized = false;

	if (!initialized)
	{
		initialized = true;

		// Read command line from /proc/self/cmdline
		int fd = open("/proc/self/cmdline", O_RDONLY);
		if (fd >= 0)
		{
			ssize_t bytes = read(fd, cmdline, sizeof(cmdline) - 1);
			close(fd);

			if (bytes > 0)
			{
				cmdline[bytes] = '\0';

				// Replace null separators with spaces (except the last one)
				for (ssize_t i = 0; i < bytes - 1; i++)
				{
					if (cmdline[i] == '\0')
					{
						cmdline[i] = ' ';
					}
				}
			}
			else
			{
				// Fallback: just use program name
				strncpy(cmdline, "UnityPlayer", sizeof(cmdline) - 1);
			}
		}
		else
		{
			// Fallback: just use program name
			strncpy(cmdline, "UnityPlayer", sizeof(cmdline) - 1);
		}
	}

	printf("	[API] Called GetCommandLineA -> \"%s\"\n", cmdline);
	return cmdline;
}

WIN_API uint16_t *Impl_GetCommandLineW()
{
	static uint16_t wcmdline[4096] = {0};
	static bool initialized = false;

	if (!initialized)
	{
		initialized = true;
		char *cl_a = Impl_GetCommandLineA();
		// Simple ASCII to UTF-16 conversion
		for (size_t i = 0; cl_a[i] != '\0' && i < 4095; i++)
		{
			wcmdline[i] = (uint16_t)(unsigned char)cl_a[i];
		}
	}

	printf("	[API] Called GetCommandLineW\n");
	return wcmdline;
}

// Windows API: BOOL CloseHandle(HANDLE hObject)
WIN_API DWORD Impl_CloseHandle(void *hObject)
{
	if (!hObject)
		return 1; // Sometimes they pass NULL, just return TRUE

	// 1. Check if it's one of our Events
	WIN_EVENT *ev = (WIN_EVENT *)hObject;
	if (ev->magic == EVENT_MAGIC)
	{
		// Destroy Linux primitives and free memory
		pthread_mutex_destroy(&ev->mutex);
		pthread_cond_destroy(&ev->cond);

		// Overwrite magic to prevent Use-After-Free bugs
		ev->magic = 0;
		free(ev);

		printf("	[API] CloseHandle (Destroyed Event: %p)\n", hObject);
		return 1; // TRUE
	}

	// 2. Check if it's one of our standard output handles (stdin/stdout)
	uintptr_t handle_val = (uintptr_t)hObject;
	if (handle_val >= (uintptr_t)STD_HANDLE_BASE && handle_val <= (uintptr_t)STD_HANDLE_BASE + 2)
	{
		printf("	[API] CloseHandle (Ignored Standard I/O Handle: %p)\n", hObject);
		return 1; // TRUE
	}

	// If we don't know what it is, just pretend we closed it successfully.
	// In a mature loader, you'd maintain a global Handle Table.
	printf("	[API] CloseHandle (Unknown Handle: %p)\n", hObject);
	return 1; // TRUE
}