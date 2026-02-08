#include "stdafx.h"

#include <utilities.h>
#include <FileOnDisk.h>

const unsigned char utf8BOM[] = { 0xEF, 0xBB, 0xBF, };
const unsigned char unicodeBOM[] = { 0xFF, 0xFE, };

//=====================================================================================================================================================================================================
//=====================================================================================================================================================================================================
Logger Logger::thelogger;


//=====================================================================================================================================================================================================
//=====================================================================================================================================================================================================
bool Logger::Open(const char *pszLogFile)
{
	this->hFile = CreateFileU(pszLogFile, GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, nullptr);

	if (INVALID_HANDLE_VALUE == this->hFile)
	{
		this->hFile = nullptr;
		return false;
	}

	DWORD dwSize = sizeof(utf8BOM);
	WriteFile(this->hFile, utf8BOM, dwSize, &dwSize, nullptr);

	SetFilePointer(this->hFile, 0, nullptr, FILE_END);
	return true;
}


//=====================================================================================================================================================================================================
//=====================================================================================================================================================================================================
bool Logger::Open(const wchar_t *pszLogFileW)
{
	std::wstring wLogFile(pszLogFileW);
	std::string sLogFile = UnicodeToUtf8(wLogFile);

	return this->Open(sLogFile.c_str());
}


//=====================================================================================================================================================================================================
//=====================================================================================================================================================================================================
bool Logger::OpenCmdScript(const char *pszLogFile)
{
	this->hCmdScript = CreateFileU(pszLogFile, GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
	if (INVALID_HANDLE_VALUE == this->hFile)
	{
		this->hCmdScript = nullptr;
		return false;
	}

	SetFilePointer(this->hCmdScript, 0, nullptr, FILE_BEGIN);
	SetEndOfFile(this->hCmdScript);

	DWORD dwSize = sizeof(utf8BOM);
	WriteFile(this->hCmdScript, utf8BOM, dwSize, &dwSize, nullptr);

	return true;
}


//=====================================================================================================================================================================================================
//=====================================================================================================================================================================================================
bool Logger::OpenCmdScript(const wchar_t *pszLogFileW)
{
	std::wstring wLogFile(pszLogFileW);
	std::string sLogFile = UnicodeToUtf8(wLogFile);
	return this->OpenCmdScript(sLogFile.c_str());
}


//=====================================================================================================================================================================================================
//=====================================================================================================================================================================================================
bool Logger::OpenPs1Script(const char *pszLogFile)
{
	this->hPs1Script = CreateFileU(pszLogFile, GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
	if (INVALID_HANDLE_VALUE == this->hFile)
	{
		this->hPs1Script = nullptr;
		return false;
	}

	SetFilePointer(this->hPs1Script, 0, nullptr, FILE_BEGIN);
	SetEndOfFile(this->hPs1Script);

	DWORD dwSize = sizeof(unicodeBOM);
	WriteFile(this->hPs1Script, unicodeBOM, dwSize, &dwSize, nullptr);

	return true;
}


//=====================================================================================================================================================================================================
//=====================================================================================================================================================================================================
bool Logger::OpenPs1Script(const wchar_t *pszLogFileW)
{
	std::wstring wLogFile(pszLogFileW);
	std::string sLogFile = UnicodeToUtf8(wLogFile);
	return this->OpenPs1Script(sLogFile.c_str());
}


//=====================================================================================================================================================================================================
//=====================================================================================================================================================================================================
bool Logger::OpenJsnScript(const char *pszLogFile)
{
	this->hJsnScript = CreateFileU(pszLogFile, GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
	if (INVALID_HANDLE_VALUE == this->hFile)
	{
		this->hJsnScript = nullptr;
		return false;
	}

	SetFilePointer(this->hJsnScript, 0, nullptr, FILE_BEGIN);
	SetEndOfFile(this->hJsnScript);

	DWORD dwSize = sizeof(utf8BOM);
	WriteFile(this->hJsnScript, utf8BOM, dwSize, &dwSize, nullptr);

	return true;
}

//=====================================================================================================================================================================================================
//=====================================================================================================================================================================================================
bool Logger::OpenJsnScript(const wchar_t *pszLogFileW)
{
	std::wstring wLogFile(pszLogFileW);
	std::string sLogFile = UnicodeToUtf8(wLogFile);
	return this->OpenJsnScript(sLogFile.c_str());
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
static std::string translate_dos(const std::string&& str)
{
	std::stringstream ss;
	char last = 0;

	for (const auto& ch : str)
	{
		//
		// CR = 13 (0x0D) (\r)
		// LF = 10 (0x0A) (\n)
		//
		if (ch == '\r') // convert CR to CRLF
		{
			ss << '\r' << '\n';
			last = ch;
		}
		else if (ch == '\n') // convert LF to CRLF
		{
			if (last == '\r')
			{
				// do nothing
			}
			else
			{
				ss << '\r' << '\n';
				last = ch;
			}
		}
		else
		{
			ss << ch;
			last = ch;
		}
	}

	return ss.str();
}


//=====================================================================================================================================================================================================
//=====================================================================================================================================================================================================
void Logger::puts(Level level, const char *pszString, DWORD dwLen)
{
	std::string str(translate_dos(pszString));
	const char *output = str.c_str();
	DWORD dwBytes = static_cast<DWORD>(str.size());

	if (0 != (level & Level::CmdScript))
	{
		WriteFile(this->hCmdScript, output, dwBytes, &dwBytes, nullptr);
	}

	if (0 != (level & Level::Ps1Script))
	{
#ifdef _DEBUG
		LARGE_INTEGER pos = { 0 };
		pos.LowPart = SetFilePointer(this->hPs1Script, pos.LowPart, &pos.HighPart, FILE_CURRENT);
		SetEndOfFile(this->hPs1Script);
#endif
		std::wstring wstr(Utf8ToUnicode(str));
		size_t len = wstr.length();
		size_t width = sizeof(wchar_t);
		size_t numBytes = len * width;
		DWORD dwWBytes = static_cast<DWORD>(numBytes);
		WriteFile(this->hPs1Script, wstr.c_str(), dwWBytes, &dwWBytes, nullptr);
	}

	if (0 != (level & Level::Json))
	{
		WriteFile(this->hJsnScript, output, dwBytes, &dwBytes, nullptr);
	}

	if (0 != (level & this->logLevel))
	{
		WriteFile(this->hFile, output, dwBytes, &dwBytes, nullptr);
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

