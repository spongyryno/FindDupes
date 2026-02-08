#include "stdafx.h"

#include <utilities.h>
#include <FileOnDisk.h>


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
void *LoadResource(HINSTANCE hInst, LPCWSTR szType, LPCWSTR szItem, DWORD *pdwSize)
{
	HRSRC	hRsrcInfo;
	HGLOBAL hGlobal;
	LPVOID	pR;
	DWORD	dwSize;

	if (!(hRsrcInfo = FindResource( hInst, szItem, szType )))
	{
		return nullptr;
	}

	dwSize=SizeofResource(hInst, hRsrcInfo);

	if (!(hGlobal = LoadResource(hInst, hRsrcInfo)))
	{
		return nullptr;
	}

	if (!(pR=LockResource(hGlobal)))
	{
		return nullptr;
	}

	if (pdwSize)
		*pdwSize = dwSize;

	return pR;
}

//=====================================================================================================================================================================================================
//=====================================================================================================================================================================================================
void *LoadResource(HINSTANCE hInst, size_t szType, size_t szItem, DWORD *pdwSize)
{
	return LoadResource(hInst, reinterpret_cast<LPCWSTR>(szType), reinterpret_cast<LPCWSTR>(szItem), pdwSize);
}


//=====================================================================================================================================================================================================
//=====================================================================================================================================================================================================
std::wstring GetLocalBinaryVersionString(void)
{
	TCHAR szModuleFileName[MAX_PATH];
	VS_FIXEDFILEINFO *	verinfo;

	GetModuleFileName(nullptr, szModuleFileName, ARRAYSIZE(szModuleFileName));

	DWORD dwSize = GetFileVersionInfoSize(szModuleFileName, 0);
	if (!dwSize)
	{
		return std::wstring();
	}

	std::vector<BYTE> buffer = std::vector<BYTE>(dwSize);

	if (!GetFileVersionInfo(szModuleFileName, 0, dwSize, &buffer[0]))
	{
		return std::wstring();
	}

	if (!VerQueryValue(&buffer[0], _T("\\"), (void **)&verinfo, (UINT *)&dwSize))
	{
		return std::wstring();
	}

	return
		std::to_wstring(HIWORD(verinfo->dwFileVersionMS)) + L"." +
		std::to_wstring(LOWORD(verinfo->dwFileVersionMS)) + L"." +
		std::to_wstring(HIWORD(verinfo->dwFileVersionLS)) + L"." +
		std::to_wstring(LOWORD(verinfo->dwFileVersionLS));
}


//=====================================================================================================================================================================================================
// Static
//=====================================================================================================================================================================================================
static HANDLE s_hTerminateProgram = nullptr;


//=====================================================================================================================================================================================================
//=====================================================================================================================================================================================================
bool ControlCHandler::TestShouldTerminate()
{
	auto result = WaitForSingleObject(s_hTerminateProgram, 0);

	if (result == WAIT_TIMEOUT)
	{
		// event was NOT signaled
		return false;
	}
	else
	{
		// event was signaled
		return true;
	}
}


//=====================================================================================================================================================================================================
//=====================================================================================================================================================================================================
static BOOL WINAPI ControlCHandler_ControlHandlerRoutine(DWORD dwCtrlType)
{
	switch (dwCtrlType)
	{
	case CTRL_BREAK_EVENT:
	case CTRL_C_EVENT:
	case CTRL_CLOSE_EVENT:
	case CTRL_LOGOFF_EVENT:
	case CTRL_SHUTDOWN_EVENT:
		if (nullptr != s_hTerminateProgram)
		{
			SetEvent(s_hTerminateProgram);
		}
		return TRUE;
		break;

	default:
		break;
	}

	return FALSE;
}


//=====================================================================================================================================================================================================
//=====================================================================================================================================================================================================
ControlCHandler::ControlCHandler()
{
	if (s_hTerminateProgram)
	{
		// this should never happen!
		__nop();
	}

	BOOL bManualReset = TRUE;
	BOOL bInitialState = FALSE;

	s_hTerminateProgram = CreateEvent(nullptr, bManualReset, bInitialState, nullptr);
	SetConsoleCtrlHandler(ControlCHandler_ControlHandlerRoutine, true);
}

//=====================================================================================================================================================================================================
//=====================================================================================================================================================================================================
ControlCHandler::~ControlCHandler()
{
	SetConsoleCtrlHandler(ControlCHandler_ControlHandlerRoutine, false);

	if (s_hTerminateProgram)
	{
		CloseHandle(s_hTerminateProgram);
		s_hTerminateProgram = nullptr;
	}
}


//=====================================================================================================================================================================================================
// Utf8 based CreateFile
//=====================================================================================================================================================================================================
static const wchar_t prefixLocal[] = L"\\\\?\\";
static const wchar_t prefixUnc[] = L"\\\\?\\UNC";

HANDLE WINAPI CreateFileU(
	_In_ LPCSTR lpFileName,
	_In_ DWORD dwDesiredAccess,
	_In_ DWORD dwShareMode,
	_In_opt_ LPSECURITY_ATTRIBUTES lpSecurityAttributes,
	_In_ DWORD dwCreationDisposition,
	_In_ DWORD dwFlagsAndAttributes,
	_In_opt_ HANDLE hTemplateFile
)
{
	const wchar_t *prefix = prefixLocal;
	const char *pStart = lpFileName;

	if ((lpFileName != nullptr) && (lpFileName[0] == '\\') && (lpFileName[0] != 0) && (lpFileName[1] == '\\'))
	{
		prefix = prefixUnc;
		pStart = &lpFileName[1];
	}

	//
	// get a Unicode version of the filespec
	//
	std::wstring wFileName = prefix + Utf8ToUnicode(pStart);

	HANDLE hFile = CreateFileW(wFileName.c_str(), dwDesiredAccess, dwShareMode, lpSecurityAttributes, dwCreationDisposition, dwFlagsAndAttributes, hTemplateFile);

	if ((hFile == nullptr) || (hFile == INVALID_HANDLE_VALUE))
	{
		__nop();
	}
	return hFile;
}


//=====================================================================================================================================================================================================
// Utf8 based FindFirstFileEx
//=====================================================================================================================================================================================================
HANDLE WINAPI FindFirstFileExU(
	_In_ LPCSTR lpFileName,
	_In_ FINDEX_INFO_LEVELS fInfoLevelId,
	_Out_writes_bytes_(sizeof(WIN32_FIND_DATAA)) LPVOID lpFindFileData,
	_In_ FINDEX_SEARCH_OPS fSearchOp,
	_Reserved_ LPVOID lpSearchFilter,
	_In_ DWORD dwAdditionalFlags
)
{
	const wchar_t *prefix = prefixLocal;
	const char *pStart = lpFileName;

	if ((lpFileName != nullptr) && (lpFileName[0] == '\\') && (lpFileName[0] != 0) && (lpFileName[1] == '\\'))
	{
		prefix = prefixUnc;
		pStart = &lpFileName[1];
	}

	//
	// get a Unicode version of the filespec
	//
	std::wstring wFileName = prefix + Utf8ToUnicode(pStart);

	//
	// convert the WIN32_FIND_DATAA to a WIN32_FIND_DATAW
	//
	WIN32_FIND_DATAA *paFindFileData = reinterpret_cast<WIN32_FIND_DATAA *>(lpFindFileData);
	WIN32_FIND_DATAW wFindFileData = {0};

	HANDLE hFind = FindFirstFileExW(wFileName.c_str(), fInfoLevelId, &wFindFileData, fSearchOp, lpSearchFilter, dwAdditionalFlags);

	if ((hFind != nullptr) && (hFind != INVALID_HANDLE_VALUE))
	{
		paFindFileData->dwFileAttributes	= wFindFileData.dwFileAttributes;
		paFindFileData->ftCreationTime		= wFindFileData.ftCreationTime;
		paFindFileData->ftLastAccessTime	= wFindFileData.ftLastAccessTime;
		paFindFileData->ftLastWriteTime		= wFindFileData.ftLastWriteTime;
		paFindFileData->nFileSizeHigh		= wFindFileData.nFileSizeHigh;
		paFindFileData->nFileSizeLow		= wFindFileData.nFileSizeLow;
		paFindFileData->dwReserved0			= wFindFileData.dwReserved0;
		paFindFileData->dwReserved1			= wFindFileData.dwReserved1;

		std::string aFileName = UnicodeToUtf8(wFindFileData.cFileName);
		std::string aAlternateFileName = UnicodeToUtf8(wFindFileData.cAlternateFileName);

		strcpy_s(paFindFileData->cFileName, _countof(paFindFileData->cFileName), aFileName.c_str());
		strcpy_s(paFindFileData->cAlternateFileName, _countof(paFindFileData->cAlternateFileName), aAlternateFileName.c_str());
	}
	else
	{
	}

	return hFind;
}

//=====================================================================================================================================================================================================
// Utf based FindNextFile
//=====================================================================================================================================================================================================
BOOL WINAPI FindNextFileU(
	_In_ HANDLE hFindFile,
	_Out_ LPWIN32_FIND_DATAA lpFindFileData
)
{
	WIN32_FIND_DATAW wFindFileData = {0};
	BOOL b = FindNextFileW(hFindFile, &wFindFileData);
	if (b)
	{
		lpFindFileData->dwFileAttributes	= wFindFileData.dwFileAttributes;
		lpFindFileData->ftCreationTime		= wFindFileData.ftCreationTime;
		lpFindFileData->ftLastAccessTime	= wFindFileData.ftLastAccessTime;
		lpFindFileData->ftLastWriteTime		= wFindFileData.ftLastWriteTime;
		lpFindFileData->nFileSizeHigh		= wFindFileData.nFileSizeHigh;
		lpFindFileData->nFileSizeLow		= wFindFileData.nFileSizeLow;
		lpFindFileData->dwReserved0			= wFindFileData.dwReserved0;
		lpFindFileData->dwReserved1			= wFindFileData.dwReserved1;

		std::string aFileName = UnicodeToUtf8(wFindFileData.cFileName);
		std::string aAlternateFileName = UnicodeToUtf8(wFindFileData.cAlternateFileName);

		strcpy_s(lpFindFileData->cFileName, _countof(lpFindFileData->cFileName), aFileName.c_str());
		strcpy_s(lpFindFileData->cAlternateFileName, _countof(lpFindFileData->cAlternateFileName), aAlternateFileName.c_str());
	}

	return b;
}


//=====================================================================================================================================================================================================
// UTF8 based Remove Directory
//=====================================================================================================================================================================================================
BOOL RemoveDirectoryU(
	_In_ LPCSTR lpPathName
)
{
	const wchar_t *prefix = prefixLocal;
	const char *pStart = lpPathName;

	if ((lpPathName != nullptr) && (lpPathName[0] == '\\') && (lpPathName[0] != 0) && (lpPathName[1] == '\\'))
	{
		prefix = prefixUnc;
		pStart = &lpPathName[1];
	}

	//
	// get a Unicode version of the filespec
	//
	std::wstring wFileName = prefix + Utf8ToUnicode(pStart);

	return RemoveDirectoryW(wFileName.c_str());
}


//=====================================================================================================================================================================================================
// UTF8 based Remove File
//=====================================================================================================================================================================================================
BOOL DeleteFileU(
	_In_ LPCSTR lpPathName,
	_In_ LPCSTR lpFileName
)
{
	const wchar_t *prefix = prefixLocal;
	const char *pStart = lpPathName;

	if ((lpPathName != nullptr) && (lpPathName[0] == '\\') && (lpPathName[0] != 0) && (lpPathName[1] == '\\'))
	{
		prefix = prefixUnc;
		pStart = &lpPathName[1];
	}

	//
	// get a Unicode version of the filespec
	//
	std::wstring wFileName = prefix + Utf8ToUnicode(pStart);
	wFileName += L'\\';
	wFileName += Utf8ToUnicode(lpFileName);

	return DeleteFileW(wFileName.c_str());
}


//=====================================================================================================================================================================================================
// UnicodeToUtf8
// Convert a Unicode wide-string C++ wstring class to a UTF-8 string inside a C++ string class, using the Windows built-in functions
//=====================================================================================================================================================================================================
std::string UnicodeToUtf8(const std::wstring& wstr)
{
	if (wstr.empty()) return std::string();

	// Calculate the size needed for the UTF-8 string
	int size_needed = WideCharToMultiByte(
		CP_UTF8,							// Convert to UTF-8
		0,									// No special flags
		wstr.c_str(),						// Source wide string
		static_cast<int>(wstr.length()),	// Source length, in characters
		nullptr,							// No output buffer yet
		0,									// Request buffer size
		nullptr, nullptr					// No default char or error flag
	);

	std::string result(size_needed, 0);

	// Perform the actual conversion
	WideCharToMultiByte(
		CP_UTF8,
		0,
		wstr.c_str(),
		static_cast<int>(wstr.length()),
		&result[0],
		size_needed,
		nullptr, nullptr
	);

	return result;
}


//=====================================================================================================================================================================================================
// Utf8ToUnicode
// Convert a UTF8 string C++ string class to a Unicode string inside a C++ wstring class, using the Windows built-in functions
//=====================================================================================================================================================================================================
std::wstring Utf8ToUnicode(const std::string& str)
{
	if (str.empty()) return std::wstring();

	// Calculate the size needed for the wide string
	int size_needed = MultiByteToWideChar(
		CP_UTF8,						// Source encoding is UTF-8
		0,								// No special flags
		str.c_str(),					// Source string
		static_cast<int>(str.length()),	// Source length
		nullptr,						// No output buffer yet
		0								// Request buffer size
	);

	std::wstring result(size_needed, 0);

	// Perform the actual conversion
	MultiByteToWideChar(
		CP_UTF8,
		0,
		str.c_str(),
		static_cast<int>(str.length()),
		&result[0],
		size_needed
	);

	return result;
}


//=====================================================================================================================================================================================================
// Escape a PowerShell string
//=====================================================================================================================================================================================================
std::string EscapePowerShellString(const char *pszInString)
{
	std::stringstream ss;

	for (const char *p=pszInString; *p; ++p)
	{
		if (*p == '\'')
		{
			ss << *p;
			ss << *p;
		}
		else if (p[0]==0x27)
		{
			// E2 80 99
			ss << *p;
			ss << *p;
		}
		else if ((p[0]=='\xE2') && (p[1]=='\x80') && (p[2]=='\x99'))
		{
			// E2 80 99
			ss << p[0];
			ss << p[1];
			ss << p[2];
			ss << *p;
		}
		else
		{
			ss << *p;
		}

	}

	return ss.str();
}

