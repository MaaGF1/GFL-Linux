#include "winapi_core.h"

// Define the thread-local error code
thread_local DWORD g_last_error = 0;

void WcharToAscii(const uint16_t *wstr, char *out_buf, size_t max_len)
{
	size_t i = 0;
	while (wstr[i] != 0 && i < max_len - 1)
	{
		// Just cast down to char. Fine for pure ASCII paths like "d3d11.dll"
		out_buf[i] = (char)(wstr[i] & 0xFF);
		i++;
	}
	out_buf[i] = '\0';
}

void AsciiToWchar(const char *str, uint16_t *wstr, size_t max_chars)
{
	size_t i = 0;
	while (str[i] != '\0' && i < max_chars - 1)
	{
		wstr[i] = (uint16_t)(unsigned char)str[i];
		i++;
	}
	wstr[i] = 0;
}