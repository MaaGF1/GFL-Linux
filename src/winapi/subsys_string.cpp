#include "subsys_string.h"

/**
 * @brief Windows API: int WideCharToMultiByte(...)
 * 
 * @note `WCHAR`: 16bit, `wchar_t`: 32bit
 */
WIN_API int Impl_WideCharToMultiByte(
	DWORD CodePage,
	DWORD dwFlags,
	const uint16_t *lpWideCharStr,
	int cchWideChar,
	char *lpMultiByteStr,
	int cbMultiByte,
	const char *lpDefaultChar,
	DWORD *lpUsedDefaultChar)
{
	// Sanity check
	if (lpWideCharStr == NULL || cchWideChar == 0)
	{
		g_last_error = 87; // ERROR_INVALID_PARAMETER
		return 0;
	}

	// 1. Determine the length of the input string
	int src_len = cchWideChar;
	if (src_len == -1)
	{
		src_len = 0;
		while (lpWideCharStr[src_len] != 0)
		{
			src_len++;
		}
		src_len++; // Include the null terminator in the count
	}

	int bytes_written = 0;
	int required_size = 0;

	// 2. UTF-16 to UTF-8 conversion loop
	for (int i = 0; i < src_len; i++)
	{
		uint32_t wc = lpWideCharStr[i];
		uint32_t codepoint = wc;

		// Handle UTF-16 Surrogate Pairs (Characters outside the BMP, e.g., Emojis)
		if (wc >= 0xD800 && wc <= 0xDBFF && (i + 1) < src_len)
		{
			uint16_t next_wc = lpWideCharStr[i + 1];
			if (next_wc >= 0xDC00 && next_wc <= 0xDFFF)
			{
				codepoint = 0x10000 + ((wc - 0xD800) << 10) + (next_wc - 0xDC00);
				i++; // Skip the low surrogate as we've consumed it
			}
		}

		// Calculate how many bytes this codepoint needs in UTF-8
		int char_bytes = 0;
		if (codepoint <= 0x7F) char_bytes = 1;
		else if (codepoint <= 0x7FF) char_bytes = 2;
		else if (codepoint <= 0xFFFF) char_bytes = 3;
		else char_bytes = 4;

		// If cbMultiByte is 0, the caller just wants to know the required buffer size
		if (cbMultiByte == 0 || lpMultiByteStr == NULL)
		{
			required_size += char_bytes;
		}
		else
		{
			// Check if we have enough space left in the buffer
			if (bytes_written + char_bytes > cbMultiByte)
			{
				g_last_error = 122; // ERROR_INSUFFICIENT_BUFFER
				return 0;
			}

			// Encode and write the UTF-8 bytes
			if (char_bytes == 1)
			{
				lpMultiByteStr[bytes_written++] = (char)codepoint;
			}
			else if (char_bytes == 2)
			{
				lpMultiByteStr[bytes_written++] = (char)(0xC0 | (codepoint >> 6));
				lpMultiByteStr[bytes_written++] = (char)(0x80 | (codepoint & 0x3F));
			}
			else if (char_bytes == 3)
			{
				lpMultiByteStr[bytes_written++] = (char)(0xE0 | (codepoint >> 12));
				lpMultiByteStr[bytes_written++] = (char)(0x80 | ((codepoint >> 6) & 0x3F));
				lpMultiByteStr[bytes_written++] = (char)(0x80 | (codepoint & 0x3F));
			}
			else if (char_bytes == 4)
			{
				lpMultiByteStr[bytes_written++] = (char)(0xF0 | (codepoint >> 18));
				lpMultiByteStr[bytes_written++] = (char)(0x80 | ((codepoint >> 12) & 0x3F));
				lpMultiByteStr[bytes_written++] = (char)(0x80 | ((codepoint >> 6) & 0x3F));
				lpMultiByteStr[bytes_written++] = (char)(0x80 | (codepoint & 0x3F));
			}
		}
	}

	// Return the calculated size if requested
	if (cbMultiByte == 0 || lpMultiByteStr == NULL)
	{
		// Don't print the actual string here to avoid console spam when size-querying
		printf("	[API] WideCharToMultiByte (Query Size) -> %d bytes needed\n", required_size);
		return required_size;
	}

	// Print the first few characters of the converted string for debugging (Environment)
	char debug_str[64] = {0};
	int copy_len = bytes_written < 63 ? bytes_written : 63;
	memcpy(debug_str, lpMultiByteStr, copy_len);

	// Replace newlines with spaces for single-line clean logging
	for(int j = 0; j < copy_len; j++)
	{
		if(debug_str[j] == '\n' || debug_str[j] == '\r') debug_str[j] = ' ';
	}

	printf("	[API] WideCharToMultiByte (Written: %d bytes) -> \"%s...\"\n", bytes_written, debug_str);
	return bytes_written;
}

// Windows API: UINT GetACP(void)
WIN_API DWORD Impl_GetACP()
{
	// By returning 65001 (UTF-8), we tell the Windows CRT to treat all standard 
	// "ANSI" strings as UTF-8. This matches Linux's native encoding perfectly!
	printf("	[API] Called GetACP -> Returning %d (CP_UTF8)\n", CP_UTF8);
	return CP_UTF8;
}

// Windows API: UINT GetOEMCP(void)
WIN_API DWORD Impl_GetOEMCP()
{
	// OEM Code Page (used for old console apps). Same as ACP for our purposes.
	printf("	[API] Called GetOEMCP -> Returning %d (CP_UTF8)\n", CP_UTF8);
	return CP_UTF8;
}

// Windows API: BOOL IsValidCodePage(UINT CodePage)
WIN_API DWORD Impl_IsValidCodePage(DWORD CodePage)
{
	// Pretend that whatever code page the engine asks for is valid, 
	// to prevent it from crashing or throwing exceptions.
	printf("	[API] Called IsValidCodePage (%u)\n", CodePage);
	return 1; // TRUE
}