// FileOnDisk.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"

#include <utilities.h>
#include <FileOnDisk.h>



//=================================================================================================
//=================================================================================================
Logger Logger::thelogger;


//=================================================================================================
//=================================================================================================
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

//=================================================================================================
//=================================================================================================
bool Logger::Open(const wchar_t *pszLogFileW)
{
	const size_t maxString = 2048;
	char pszLogFileA[maxString];

	WideCharToMultiByte(CP_ACP, 0, pszLogFileW, -1, pszLogFileA, sizeof(pszLogFileA), nullptr, nullptr);
	return this->Open(pszLogFileA);
}

//=================================================================================================
//=================================================================================================
bool Logger::OpenCmdScript(const char *pszLogFile)
{
	this->hDeleteScript = CreateFileA(pszLogFile, GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
	if (INVALID_HANDLE_VALUE == this->hFile)
	{
		this->hDeleteScript = nullptr;
		return false;
	}

	SetFilePointer(this->hDeleteScript, 0, nullptr, FILE_BEGIN);
	SetEndOfFile(this->hDeleteScript);
	return true;
}

//=================================================================================================
//=================================================================================================
bool Logger::OpenCmdScript(const wchar_t *pszLogFileW)
{
	const size_t maxString = 2048;
	char pszLogFileA[maxString];

	WideCharToMultiByte(CP_ACP, 0, pszLogFileW, -1, pszLogFileA, sizeof(pszLogFileA), nullptr, nullptr);
	return this->OpenCmdScript(pszLogFileA);
}

//=================================================================================================
//=================================================================================================
void Logger::Reset()
{
	SetFilePointer(this->hFile, 0, nullptr, FILE_BEGIN);
	SetEndOfFile(this->hFile);
}



//=================================================================================================
//=================================================================================================
void Logger::Close()
{
	SafeCloseHandle(this->hDeleteScript);
	SafeCloseHandle(this->hFile);
}

//=================================================================================================
//=================================================================================================
void Logger::printf(Level level, const char *pszFormat, ...)
{
	if (0 == (level & (this->outLevel | this->logLevel | Level::DeleteScript)))
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

	if (0 != (level & Level::DeleteScript))
	{
		DWORD dwBytes;
		WriteFile(this->hDeleteScript, output, static_cast<DWORD>(strlen(output)), &dwBytes, nullptr);
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

	return;
}

