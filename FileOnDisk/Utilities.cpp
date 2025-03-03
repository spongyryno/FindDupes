#include "stdafx.h"

#include <utilities.h>
#include <FileOnDisk.h>



//=====================================================================================================================================================================================================
//=====================================================================================================================================================================================================
Logger Logger::thelogger;


//=====================================================================================================================================================================================================
//=====================================================================================================================================================================================================
bool Logger::Open(const char *pszLogFile)
{
	this->hFile = CreateFileA(pszLogFile, GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
	if (INVALID_HANDLE_VALUE == this->hFile)
	{
		this->hFile = nullptr;
		return false;
	}

	SetFilePointer(this->hFile, 0, nullptr, FILE_END);
	return true;
}

//=====================================================================================================================================================================================================
//=====================================================================================================================================================================================================
bool Logger::Open(const wchar_t *pszLogFileW)
{
	const size_t maxString = 2048;
	char pszLogFileA[maxString];

	WideCharToMultiByte(CP_ACP, 0, pszLogFileW, -1, pszLogFileA, sizeof(pszLogFileA), nullptr, nullptr);
	return this->Open(pszLogFileA);
}

//=====================================================================================================================================================================================================
//=====================================================================================================================================================================================================
bool Logger::OpenCmdScript(const char *pszLogFile)
{
	this->hCmdScript = CreateFileA(pszLogFile, GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
	if (INVALID_HANDLE_VALUE == this->hFile)
	{
		this->hCmdScript = nullptr;
		return false;
	}

	SetFilePointer(this->hCmdScript, 0, nullptr, FILE_BEGIN);
	SetEndOfFile(this->hCmdScript);
	return true;
}

//=====================================================================================================================================================================================================
//=====================================================================================================================================================================================================
bool Logger::OpenCmdScript(const wchar_t *pszLogFileW)
{
	const size_t maxString = 2048;
	char pszLogFileA[maxString];

	WideCharToMultiByte(CP_ACP, 0, pszLogFileW, -1, pszLogFileA, sizeof(pszLogFileA), nullptr, nullptr);
	return this->OpenCmdScript(pszLogFileA);
}

//=====================================================================================================================================================================================================
//=====================================================================================================================================================================================================
bool Logger::OpenPs1Script(const char *pszLogFile)
{
	this->hPs1Script = CreateFileA(pszLogFile, GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
	if (INVALID_HANDLE_VALUE == this->hFile)
	{
		this->hPs1Script = nullptr;
		return false;
	}

	SetFilePointer(this->hPs1Script, 0, nullptr, FILE_BEGIN);
	SetEndOfFile(this->hPs1Script);
	return true;
}

//=====================================================================================================================================================================================================
//=====================================================================================================================================================================================================
bool Logger::OpenPs1Script(const wchar_t *pszLogFileW)
{
	const size_t maxString = 2048;
	char pszLogFileA[maxString];

	WideCharToMultiByte(CP_ACP, 0, pszLogFileW, -1, pszLogFileA, sizeof(pszLogFileA), nullptr, nullptr);
	return this->OpenPs1Script(pszLogFileA);
}

//=====================================================================================================================================================================================================
//=====================================================================================================================================================================================================
bool Logger::OpenJsnScript(const char *pszLogFile)
{
	this->hJsnScript = CreateFileA(pszLogFile, GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
	if (INVALID_HANDLE_VALUE == this->hFile)
	{
		this->hJsnScript = nullptr;
		return false;
	}

	SetFilePointer(this->hJsnScript, 0, nullptr, FILE_BEGIN);
	SetEndOfFile(this->hJsnScript);
	return true;
}

//=====================================================================================================================================================================================================
//=====================================================================================================================================================================================================
bool Logger::OpenJsnScript(const wchar_t *pszLogFileW)
{
	const size_t maxString = 2048;
	char pszLogFileA[maxString];

	WideCharToMultiByte(CP_ACP, 0, pszLogFileW, -1, pszLogFileA, sizeof(pszLogFileA), nullptr, nullptr);
	return this->OpenJsnScript(pszLogFileA);
}

//=====================================================================================================================================================================================================
//=====================================================================================================================================================================================================
void Logger::Reset()
{
	SetFilePointer(this->hFile, 0, nullptr, FILE_BEGIN);
	SetEndOfFile(this->hFile);
}



//=====================================================================================================================================================================================================
//=====================================================================================================================================================================================================
void Logger::Close()
{
	SafeCloseHandle(this->hJsnScript);
	SafeCloseHandle(this->hPs1Script);
	SafeCloseHandle(this->hCmdScript);
	SafeCloseHandle(this->hFile);
}

//=====================================================================================================================================================================================================
//=====================================================================================================================================================================================================
static inline void strcpy_translate_dos(const char *src, char *dst, size_t dstsizechars)
{
	char *p = dst;

	while (*src && (p < &dst[dstsizechars-1]))
	{
		if (*src == '\n')
		{
			*p ++ = '\r';
		}
		else if (*src == '\r' && src[1] == '\n')
		{
			*p++ = *src++;
		}

		*p++ = *src++;
	}

	*p = 0;
}

//=====================================================================================================================================================================================================
//=====================================================================================================================================================================================================
void Logger::puts(Level level, const char *pszString, DWORD dwLen)
{
	const size_t maxLine = 16384;
	char output[maxLine];

	if (0 == dwLen)
	{
		dwLen = ARRAYSIZE(output);
	}

	strcpy_translate_dos(pszString, output, dwLen);

	if (0 != (level & Level::CmdScript))
	{
		DWORD dwBytes;
		WriteFile(this->hCmdScript, output, static_cast<DWORD>(strlen(output)), &dwBytes, nullptr);
	}

	if (0 != (level & Level::Ps1Script))
	{
		DWORD dwBytes;
		WriteFile(this->hPs1Script, output, static_cast<DWORD>(strlen(output)), &dwBytes, nullptr);
	}

	if (0 != (level & Level::Json))
	{
		DWORD dwBytes;
		WriteFile(this->hJsnScript, output, static_cast<DWORD>(strlen(output)), &dwBytes, nullptr);
	}

	if (0 != (level & this->logLevel))
	{
		DWORD dwBytes;
		WriteFile(this->hFile, output, static_cast<DWORD>(strlen(output)), &dwBytes, nullptr);
	}

	if (0 != (level & this->outLevel))
	{
		fputs(output, stdout);
	}

}

//=====================================================================================================================================================================================================
//=====================================================================================================================================================================================================
void Logger::printf(Level level, const char *pszFormat, ...)
{
	// does the current log level care about this message?
	if (0 == (level & (this->outLevel | this->logLevel | Level::CmdScript | Level::Ps1Script | Level::Json)))
	{
		return;
	}

	va_list arglist;
	const size_t maxLine = 16384;
	char output[maxLine];
	int retval;

	va_start(arglist, pszFormat);
	retval = vsprintf_s(output, ARRAYSIZE(output), pszFormat, arglist);
	va_end(arglist);

	this->puts(level, output, 0);
	return;
}

