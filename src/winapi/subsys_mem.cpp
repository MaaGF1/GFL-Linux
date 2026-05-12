#include "subsys_mem.h"

#define MAX_VIRTUAL_ALLOCS 4096
typedef struct
{
	void *addr;
	size_t size;
} VIRTUAL_ALLOC_RECORD;

static VIRTUAL_ALLOC_RECORD g_valloc_tracker[MAX_VIRTUAL_ALLOCS];
static pthread_mutex_t g_valloc_mutex = PTHREAD_MUTEX_INITIALIZER;

static void TrackVirtualAlloc(void *addr, size_t size)
{
	pthread_mutex_lock(&g_valloc_mutex);
	for (int i = 0; i < MAX_VIRTUAL_ALLOCS; i++)
	{
		if (g_valloc_tracker[i].addr == NULL)
		{
			g_valloc_tracker[i].addr = addr;
			g_valloc_tracker[i].size = size;
			break;
		}
	}
	pthread_mutex_unlock(&g_valloc_mutex);
}

static size_t UntrackVirtualAlloc(void *addr)
{
	size_t size = 0;
	pthread_mutex_lock(&g_valloc_mutex);
	for (int i = 0; i < MAX_VIRTUAL_ALLOCS; i++)
	{
		if (g_valloc_tracker[i].addr == addr)
		{
			size = g_valloc_tracker[i].size;
			g_valloc_tracker[i].addr = NULL;
			g_valloc_tracker[i].size = 0;
			break;
		}
	}
	pthread_mutex_unlock(&g_valloc_mutex);
	return size;
}

static int WinProtToLinuxProt(DWORD flProtect)
{
	if (flProtect & PAGE_EXECUTE_READWRITE)
		return PROT_READ | PROT_WRITE | PROT_EXEC;
	if (flProtect & PAGE_EXECUTE_READ)
		return PROT_READ | PROT_EXEC;
	if (flProtect & PAGE_EXECUTE)
		return PROT_EXEC;
	if (flProtect & PAGE_READWRITE)
		return PROT_READ | PROT_WRITE;
	if (flProtect & PAGE_READONLY)
		return PROT_READ;
	if (flProtect == PAGE_NOACCESS)
		return PROT_NONE;
	return PROT_READ | PROT_WRITE; // Fallback
}

// Windows API: LPVOID VirtualAlloc(LPVOID lpAddress, SIZE_T dwSize, DWORD flAllocationType, DWORD flProtect)
WIN_API void *Impl_VirtualAlloc(void *lpAddress, size_t dwSize, DWORD flAllocationType, DWORD flProtect)
{
	int linux_prot = WinProtToLinuxProt(flProtect);
	int linux_flags = MAP_PRIVATE | MAP_ANONYMOUS;

	// Handle memory allocation
	// If lpAddress is provided, Windows attempts to allocate at that specific address.
	// In mmap, providing an address is just a "hint" unless MAP_FIXED is used.
	// We avoid MAP_FIXED because it overwrites existing memory if it overlaps, which is dangerous.
	void *ptr = mmap(lpAddress, dwSize, linux_prot, linux_flags, -1, 0);

	if (ptr == MAP_FAILED)
	{
		g_last_error = 8; // ERROR_NOT_ENOUGH_MEMORY
		printf("	[API] VirtualAlloc (Size: %zu, Prot: 0x%X) -> FAILED!\n", dwSize, (unsigned int)flProtect);
		return NULL;
	}

	// Record size for VirtualFree
	TrackVirtualAlloc(ptr, dwSize);

	printf("	[API] VirtualAlloc (Size: %zu, Prot: 0x%X) -> %p\n", dwSize, (unsigned int)flProtect, ptr);
	return ptr;
}

// Windows API: BOOL VirtualFree(LPVOID lpAddress, SIZE_T dwSize, DWORD dwFreeType)
WIN_API DWORD Impl_VirtualFree(void *lpAddress, size_t dwSize, DWORD dwFreeType)
{
	if (dwFreeType & MEM_RELEASE)
	{
		// Windows documentation: If MEM_RELEASE, dwSize MUST be 0.
		size_t tracked_size = UntrackVirtualAlloc(lpAddress);
		if (tracked_size > 0)
		{
			munmap(lpAddress, tracked_size);
			printf("	[API] VirtualFree (Release, Addr: %p, Tracked Size: %zu) -> OK\n", lpAddress, tracked_size);
			return 1;
		}
		printf("	[API] VirtualFree (Release, Addr: %p) -> FAILED (Not Tracked)\n", lpAddress);
		return 0;
	}
	else if (dwFreeType & MEM_DECOMMIT)
	{
		// Simulate Decommit by removing all access
		mprotect(lpAddress, dwSize, PROT_NONE);
		printf("	[API] VirtualFree (Decommit, Addr: %p, Size: %zu) -> OK\n", lpAddress, dwSize);
		return 1;
	}

	return 0;
}

// Windows API: BOOL VirtualProtect(LPVOID lpAddress, SIZE_T dwSize, DWORD flNewProtect, PDWORD lpflOldProtect)
WIN_API DWORD Impl_VirtualProtect(void *lpAddress, size_t dwSize, DWORD flNewProtect, DWORD *lpflOldProtect)
{
	// Provide a dummy old protect value to keep the caller happy
	if (lpflOldProtect)
	{
		*lpflOldProtect = PAGE_READWRITE;
	}

	// mprotect requires the address to be exactly page-aligned
	uintptr_t page_mask = ~((uintptr_t)sysconf(_SC_PAGESIZE) - 1);
	void *aligned_addr = (void *)((uintptr_t)lpAddress & page_mask);

	// Stretch the size to cover the difference caused by aligning the address down
	size_t aligned_size = dwSize + ((uintptr_t)lpAddress - (uintptr_t)aligned_addr);

	int linux_prot = WinProtToLinuxProt(flNewProtect);
	int ret = mprotect(aligned_addr, aligned_size, linux_prot);

	printf("	[API] VirtualProtect (%p, Size: %zu, NewProt: 0x%X) -> %s\n", lpAddress, dwSize, flNewProtect, ret == 0 ? "OK" : "FAILED");

	return ret == 0 ? 1 : 0;
}

// Windows API: SIZE_T VirtualQuery(LPCVOID lpAddress, PMEMORY_BASIC_INFORMATION lpBuffer, SIZE_T dwLength)
WIN_API size_t Impl_VirtualQuery(void *lpAddress, MEMORY_BASIC_INFORMATION *lpBuffer, size_t dwLength)
{
	if (dwLength < sizeof(MEMORY_BASIC_INFORMATION))
		return 0;

	memset(lpBuffer, 0, sizeof(MEMORY_BASIC_INFORMATION));

	// Default to FREE if we don't find it
	lpBuffer->State = MEM_FREE;
	lpBuffer->Protect = PAGE_NOACCESS;
	lpBuffer->BaseAddress = (void *)((uintptr_t)lpAddress & ~0xFFFULL);
	lpBuffer->RegionSize = 0x1000;

	// Parse Linux /proc/self/maps to give highly accurate memory state to Unity's GC
	FILE *fp = fopen("/proc/self/maps", "r");
	if (fp)
	{
		char line[512];
		while (fgets(line, sizeof(line), fp))
		{
			uintptr_t start, end;
			char perms[8];

			if (sscanf(line, "%lx-%lx %7s", &start, &end, perms) == 3)
			{
				if ((uintptr_t)lpAddress >= start && (uintptr_t)lpAddress < end)
				{
					lpBuffer->BaseAddress = (void *)start;
					lpBuffer->AllocationBase = (void *)start;
					lpBuffer->RegionSize = end - start;
					lpBuffer->State = MEM_COMMIT;
					lpBuffer->Type = MEM_PRIVATE;

					DWORD prot = 0;
					if (perms[0] == 'r')
						prot |= PAGE_READONLY;
					if (perms[1] == 'w')
						prot = PAGE_READWRITE; // override

					if (perms[2] == 'x')
					{
						if (prot == PAGE_READWRITE)
							prot = PAGE_EXECUTE_READWRITE;
						else if (prot == PAGE_READONLY)
							prot = PAGE_EXECUTE_READ;
						else
							prot = PAGE_EXECUTE;
					}

					if (prot == 0)
						prot = PAGE_NOACCESS;

					lpBuffer->Protect = prot;
					lpBuffer->AllocationProtect = prot;
					break;
				}
			}
		}
		fclose(fp);
	}

	// Prevent console spam as this is called thousands of times per second by the GC
	// printf("	[API] VirtualQuery (%p) -> State: 0x%X\n", lpAddress, lpBuffer->State);

	return sizeof(MEMORY_BASIC_INFORMATION);
}

// Windows API: HANDLE GetProcessHeap(void)
WIN_API void *Impl_GetProcessHeap()
{
	static void *process_heap_handle = PROCESS_HEAP_MAGIC;
	printf("	[API] Called GetProcessHeap -> Handle: %p\n", process_heap_handle);
	return process_heap_handle;
}

// Windows API: LPVOID HeapAlloc(HANDLE hHeap, DWORD dwFlags, SIZE_T dwBytes)
WIN_API void *Impl_HeapAlloc(void *hHeap, DWORD dwFlags, size_t dwBytes)
{
	void *ptr = NULL;

	if (hHeap == PROCESS_HEAP_MAGIC)
	{
		if (dwFlags & HEAP_ZERO_MEMORY)
		{
			ptr = calloc(1, dwBytes);
		}
		else
		{
			ptr = malloc(dwBytes);
		}

		static bool logged_first_heap_alloc = false;
		if (!logged_first_heap_alloc)
		{
			printf("	[API] HeapAlloc (ProcessHeap, Size: %zu) -> %p\n", dwBytes, ptr);
			logged_first_heap_alloc = true;
		}
	}
	return ptr;
}

// Windows API: BOOL HeapFree(HANDLE hHeap, DWORD dwFlags, LPVOID lpMem)
WIN_API DWORD Impl_HeapFree(void *hHeap, DWORD dwFlags, void *lpMem)
{
	if (hHeap == PROCESS_HEAP_MAGIC)
	{
		free(lpMem);
		// Do not print here, it is called too frequently
		return 1; // TRUE
	}
	return 0; // FALSE
}