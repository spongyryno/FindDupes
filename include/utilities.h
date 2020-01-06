//=================================================================================================
//=================================================================================================
#define verboseprintf(fmt, ...) if (verbose) { Logger::Get().printf(Logger::Level::Info, fmt, __VA_ARGS__); }

//=================================================================================================
//=================================================================================================

//=================================================================================================
//=================================================================================================
template<typename _Ty> inline bool fexists(const _Ty *filename)
{
	static_assert(0);
}

//=================================================================================================
//=================================================================================================
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

//=================================================================================================
//=================================================================================================
template<> inline bool fexists<char>(const char *filename)
{
	HANDLE hFile = CreateFileA(filename, 0, 0, nullptr, OPEN_EXISTING, 0, nullptr);
	if ((nullptr != hFile) && (INVALID_HANDLE_VALUE != hFile))
	{
		CloseHandle(hFile);
		return true;
	}

	return false;
}

//=================================================================================================
//=================================================================================================
inline FILETIME GetFileTimeStamp(const char *pszFilePath)
{
	FILETIME null{ 0 };

	HANDLE hFile = CreateFileA(pszFilePath, 0, 0, nullptr, OPEN_EXISTING, 0, nullptr);
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

//=================================================================================================
//=================================================================================================
inline long long FileTimeToUInt64(const FILETIME &filetime)
{
	return *reinterpret_cast<const long long *>(&filetime.dwLowDateTime);
}


//=================================================================================================
//=================================================================================================
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
};



//=================================================================================================
//=================================================================================================
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
		QueryPerformanceCounter(&time2);
		double seconds = (double)(time2.QuadPart - time1.QuadPart)/(double)freq.QuadPart;
		Logger::Get().printf(Logger::Level::Debug, "It took %15.5f seconds (%s)\n", seconds, this->desc);
	}

private:
	LARGE_INTEGER freq;
	LARGE_INTEGER time1;
	LARGE_INTEGER time2;
	const char *desc;
};


//=================================================================================================
//=================================================================================================
inline bool operator ==(const FILETIME &left, const FILETIME &right)
{
	return (left.dwHighDateTime == right.dwHighDateTime) && (left.dwLowDateTime == right.dwLowDateTime);
}

//=================================================================================================
//=================================================================================================
inline unsigned long long FileTimeDifference(const FILETIME &ft1, const FILETIME &ft2)
{
	long long diff = (*reinterpret_cast<const long long *>(&ft1)) - (*reinterpret_cast<const long long *>(&ft2));
	return diff < 0 ? -diff : diff;
}

const long long ftMilliseconds = 10000;


//=================================================================================================
//=================================================================================================
inline void SafeCloseHandle(HANDLE &handle)
{
	if (nullptr != handle)
	{
		CloseHandle(handle);
		handle = nullptr;
	}
}


//=================================================================================================
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
//=================================================================================================
__declspec(noinline) inline char *comma(unsigned __int64 n)
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

//=================================================================================================
//=================================================================================================
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
	}

	return false;
}



