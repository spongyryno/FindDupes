//=====================================================================================================================================================================================================
//=====================================================================================================================================================================================================
#define verboseprintf(fmt, ...) if (verbose) { Logger::Get().printf(Logger::Level::Info, fmt, __VA_ARGS__); }


//=====================================================================================================================================================================================================
// UTF8 based CreateFile
//=====================================================================================================================================================================================================
extern HANDLE WINAPI CreateFileU(
	_In_ LPCSTR lpFileName,
	_In_ DWORD dwDesiredAccess,
	_In_ DWORD dwShareMode,
	_In_opt_ LPSECURITY_ATTRIBUTES lpSecurityAttributes,
	_In_ DWORD dwCreationDisposition,
	_In_ DWORD dwFlagsAndAttributes,
	_In_opt_ HANDLE hTemplateFile
);


//=====================================================================================================================================================================================================
// Utf8 based FindFirstFileEx
//=====================================================================================================================================================================================================
extern HANDLE WINAPI FindFirstFileExU(
	_In_ LPCSTR lpFileName,
	_In_ FINDEX_INFO_LEVELS fInfoLevelId,
	_Out_writes_bytes_(sizeof(WIN32_FIND_DATAA)) LPVOID lpFindFileData,
	_In_ FINDEX_SEARCH_OPS fSearchOp,
	_Reserved_ LPVOID lpSearchFilter,
	_In_ DWORD dwAdditionalFlags
);


//=====================================================================================================================================================================================================
// Utf based FindNextFile
//=====================================================================================================================================================================================================
extern BOOL WINAPI FindNextFileU(
	_In_ HANDLE hFindFile,
	_Out_ LPWIN32_FIND_DATAA lpFindFileData
);


//=====================================================================================================================================================================================================
// UTF8 based Remove Directory
//=====================================================================================================================================================================================================
extern BOOL RemoveDirectoryU(
	_In_ LPCSTR lpPathName
);


//=====================================================================================================================================================================================================
// UTF8 based Remove File
//=====================================================================================================================================================================================================
extern BOOL DeleteFileU(
	_In_ LPCSTR lpPathName,
	_In_ LPCSTR lpFileName
);


//=====================================================================================================================================================================================================
//=====================================================================================================================================================================================================
template<typename _Ty> inline bool fexists(const _Ty *filename)
{
	//static_assert(0);
	return false;
}

//=====================================================================================================================================================================================================
//=====================================================================================================================================================================================================
template<> inline bool fexists<wchar_t>(const wchar_t *filename)
{
	HANDLE hFile = CreateFileW(filename, 0, 0, nullptr, OPEN_EXISTING, 0, nullptr);
	if ((nullptr != hFile) && (INVALID_HANDLE_VALUE != hFile))
	{
		CloseHandle(hFile);
		return true;
	}

	return false;
}

//=====================================================================================================================================================================================================
//=====================================================================================================================================================================================================
template<> inline bool fexists<char>(const char *filename)
{
	HANDLE hFile = CreateFileU(filename, 0, 0, nullptr, OPEN_EXISTING, 0, nullptr);
	if ((nullptr != hFile) && (INVALID_HANDLE_VALUE != hFile))
	{
		CloseHandle(hFile);
		return true;
	}

	return false;
}

//=====================================================================================================================================================================================================
//=====================================================================================================================================================================================================
inline FILETIME GetFileTimeStamp(const char *pszFilePath)
{
	FILETIME null{ 0 };

	HANDLE hFile = CreateFileU(pszFilePath, 0, 0, nullptr, OPEN_EXISTING, 0, nullptr);
	if ((nullptr != hFile) && (INVALID_HANDLE_VALUE != hFile))
	{
		BY_HANDLE_FILE_INFORMATION fi{0};

		if (GetFileInformationByHandle(hFile, &fi))
		{
			CloseHandle(hFile);
			return fi.ftLastWriteTime;
		}

		CloseHandle(hFile);
		return null;
	}

	return null;
}

//=====================================================================================================================================================================================================
//=====================================================================================================================================================================================================
inline long long FileTimeToUInt64(const FILETIME &filetime)
{
	return *reinterpret_cast<const long long *>(&filetime.dwLowDateTime);
}


//=====================================================================================================================================================================================================
//=====================================================================================================================================================================================================
inline wchar_t *GetLastErrorString()
{
	DWORD dwLastError = GetLastError();
	static wchar_t szMessage[MAX_PATH] = {0};
	FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, NULL, dwLastError, 0, szMessage, _countof(szMessage), 0);
	return &szMessage[0];
}


//=====================================================================================================================================================================================================
//=====================================================================================================================================================================================================
class Logger
{
public:
	enum Level : unsigned int
	{
		Error			= 0x01,
		Info			= 0x02,
		Warning			= 0x04,
		Debug			= 0x08,
		Dupes			= 0x10,
		CmdScript		= 0x20,
		Ps1Script		= 0x40,
		Json			= 0x80,

		Log_Default	= Error|Info|Warning|Debug|Dupes,
		Out_Default = Error|Info|Warning,

		All			= 0xFFFFFFFF,
	};

public:
	static Logger &Get() { return thelogger; }

	void SetLogLevel(Level level) { this->logLevel = level; }
	void SetOutLevel(Level level) { this->outLevel = level; }
	bool Open(const char *pszLogFile);
	bool Open(const wchar_t *pszLogFile);
	bool OpenCmdScript(const char *pszLogFile);
	bool OpenCmdScript(const wchar_t *pszLogFile);
	bool OpenPs1Script(const char *pszLogFile);
	bool OpenPs1Script(const wchar_t *pszLogFile);
	bool OpenJsnScript(const char *pszLogFile);
	bool OpenJsnScript(const wchar_t *pszLogFile);
	void Close();
	void Reset();
	void printf(Level level, const char *pszFormat, ...);

	void puts(Level level, const char *pszString, DWORD dwLen);

	Logger() : logLevel(Log_Default), outLevel(Out_Default), hFile(nullptr), hCmdScript(nullptr), hPs1Script(nullptr) {}
	~Logger() { this->Close(); }

private:
	static Logger	thelogger;
	Level			logLevel;
	Level			outLevel;
	HANDLE			hFile;
	HANDLE			hCmdScript;
	HANDLE			hPs1Script;
	HANDLE			hJsnScript;
};



//=====================================================================================================================================================================================================
//=====================================================================================================================================================================================================
class TimeThis
{
public:
	TimeThis()
	{
		this->desc = "";
		QueryPerformanceFrequency(&freq);
		QueryPerformanceCounter(&time1);
	}

	TimeThis(const char *pDesc)
	{
		this->desc = pDesc;
		QueryPerformanceFrequency(&freq);
		QueryPerformanceCounter(&time1);
	}

	~TimeThis()
	{
		double seconds = this->Elapsed();
		Logger::Get().printf(Logger::Level::Debug, "It took %15.5f seconds (%s)\n", seconds, this->desc);
	}

	double Elapsed() const
	{
		LARGE_INTEGER time2;
		QueryPerformanceCounter(&time2);
		double seconds = (double)(time2.QuadPart - time1.QuadPart) / (double)freq.QuadPart;
		return seconds;
	}

private:
	LARGE_INTEGER freq;
	LARGE_INTEGER time1;
	const char *desc;
};


//=====================================================================================================================================================================================================
//=====================================================================================================================================================================================================
inline bool operator ==(const FILETIME &left, const FILETIME &right)
{
	return (left.dwHighDateTime == right.dwHighDateTime) && (left.dwLowDateTime == right.dwLowDateTime);
}

//=====================================================================================================================================================================================================
//=====================================================================================================================================================================================================
inline unsigned long long FileTimeDifference(const FILETIME &ft1, const FILETIME &ft2)
{
	long long diff = (*reinterpret_cast<const long long *>(&ft1)) - (*reinterpret_cast<const long long *>(&ft2));
	return diff < 0 ? -diff : diff;
}

const long long ftMilliseconds = 10000;


//=====================================================================================================================================================================================================
//=====================================================================================================================================================================================================
inline void SafeCloseHandle(HANDLE &handle)
{
	if (nullptr != handle)
	{
		CloseHandle(handle);
		handle = nullptr;
	}
}


//=====================================================================================================================================================================================================
//
// comma
//
// Given an integer, create a comma-delimited string representation of
// that integer in decimal form. For example, one million, 1000000,
// creates a string "1,000,000"
//
// in:
//  n				The integer
//
// out:
//  return value	A pointer to a static string
//
//=====================================================================================================================================================================================================
__declspec(noinline) inline const char *comma(unsigned __int64 n)
{
	const int numStrings = 8; // must be a power of two!!
	const int stringLength = 256;
	static char string[numStrings][stringLength];
	static int	index=-1;
	char *p;

	// make sure the numstrings is a power of two
	C_ASSERT(0 == (numStrings & (numStrings - 1)));

	index = (index+1)&(numStrings-1);

	p = &string[index][sizeof(string[index]) - 1];
	*p-- = 0;

	int commacount = 0;

	if (!n)
		return "0";

	while (n)
	{
		if (commacount == 3)
		{
			*p-- = ',';
			commacount = 0;
		}

		*p-- = '0' + (char)(n % 10); n /= 10;
		commacount++;
	}

	return ++p;

}

//=====================================================================================================================================================================================================
//=====================================================================================================================================================================================================
inline bool RelativeToFullpath(char *szPath, size_t lenInCharacters)
{
	const size_t maxString = 1024;
	char szCurrentFolder[maxString];

	if (0 != GetCurrentDirectoryA(ARRAYSIZE(szCurrentFolder), szCurrentFolder))
	{
		if (0 != SetCurrentDirectoryA(szPath))
		{
			if (0 != GetCurrentDirectoryA(static_cast<DWORD>(lenInCharacters), szPath))
			{
				// remove any trailing backslash
				if (szPath[strlen(szPath) - 1] == '\\')
				{
					szPath[strlen(szPath) - 1] = 0;
				}

				if (0 != SetCurrentDirectoryA(szCurrentFolder))
				{
					return true;
				}
			}
		}
		else
		{
			Logger::Get().printf(Logger::Level::Error, "Error changing to path \"%s\"! (%S, %d)\n", szPath, GetLastErrorString(), GetLastError());
			__nop();
		}
	}

	return false;
}



//=====================================================================================================================================================================================================
// Escape a string for Json
//=====================================================================================================================================================================================================
template<typename _Ty> inline const _Ty* EscapeJsonString(const _Ty *str)
{
	return nullptr;
}


//=====================================================================================================================================================================================================
// Escape a string for Json
//=====================================================================================================================================================================================================
template<> inline const wchar_t* EscapeJsonString<wchar_t>(const wchar_t *str)
{
	static wchar_t output[MAX_PATH];
	wchar_t *p = &output[0];

	const wchar_t *ip = str;
	wchar_t *op = &output[0];

	while (*ip)
	{
		if (*ip == L'\\')
		{
			*op++ = L'\\';
		}

		*op++ = *ip++;
	}

	*op = 0;

	return &output[0];
}


//=====================================================================================================================================================================================================
// Escape a string for Json
//=====================================================================================================================================================================================================
template<> inline const char* EscapeJsonString<char>(const char *str)
{
	static char output[MAX_PATH];
	char *p = &output[0];

	const char *ip = str;
	char *op = &output[0];

	while (*ip)
	{
		if (*ip == '\\')
		{
			*op++ = '\\';
		}

		*op++ = *ip++;
	}

	*op = 0;

	return &output[0];
}


//=====================================================================================================================================================================================================
//=====================================================================================================================================================================================================
class ControlCHandler
{
public:
	ControlCHandler();
	~ControlCHandler();
	static bool TestShouldTerminate();

private:
};


//=====================================================================================================================================================================================================
//
// LoadResource
//
// Read a resource into memory, return a pointer to it, and optionally
// return the size of the resource
//
// in:
//	hInst		handle to the application instance
//	szItem		the resource identifier
//	pdwSize		pointer to size
//
// out:
//	LPVOID		pointer to the memory
//
//=====================================================================================================================================================================================================
extern void *LoadResource(HINSTANCE hInst, LPCWSTR szType, LPCWSTR szItem, DWORD *pdwSize=nullptr);
extern void *LoadResource(HINSTANCE hInst, size_t szType, size_t szItem, DWORD *pdwSize=nullptr);
extern std::wstring GetLocalBinaryVersionString(void);


//=====================================================================================================================================================================================================
// Conversion functions for Unicode/UTF-8
//=====================================================================================================================================================================================================
extern std::string UnicodeToUtf8(const std::wstring& wstr);
extern std::wstring Utf8ToUnicode(const std::string& str);

extern std::string EscapePowerShellString(const char *pszInString);
