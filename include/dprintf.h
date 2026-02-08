//=====================================================================================================================================================================================================
//=====================================================================================================================================================================================================
__declspec(noinline) inline bool IsConsoleApp(void)
{
	HMODULE hMainModule;
	PIMAGE_DOS_HEADER pDosHeader;
	PIMAGE_NT_HEADERS pImageNtHeaders;
	WORD wSubsystem;
	static bool cachedAnswer;
	static bool isCached = false;

	if (!isCached)
	{
		isCached = true;
		hMainModule = GetModuleHandle(nullptr);
		pDosHeader = reinterpret_cast<PIMAGE_DOS_HEADER>(hMainModule);
		pImageNtHeaders = reinterpret_cast<PIMAGE_NT_HEADERS>(reinterpret_cast<size_t>(pDosHeader) + pDosHeader->e_lfanew);
		wSubsystem = pImageNtHeaders->OptionalHeader.Subsystem;
		cachedAnswer = (wSubsystem == IMAGE_SUBSYSTEM_WINDOWS_CUI);
	}

	return cachedAnswer;
}

//=====================================================================================================================================================================================================
//=====================================================================================================================================================================================================
template<typename _Ty> inline void OutputDebugStringTemplate(const _Ty *lpOutputString)
{
}

//=====================================================================================================================================================================================================
//=====================================================================================================================================================================================================
template<> inline void OutputDebugStringTemplate<wchar_t>(const wchar_t *lpOutputString)
{
	OutputDebugStringW(lpOutputString);
	//fputws(lpOutputString, stdout);
}

//=====================================================================================================================================================================================================
//=====================================================================================================================================================================================================
template<> inline void OutputDebugStringTemplate<char>(const char *lpOutputString)
{
	OutputDebugStringA(lpOutputString);
	//fputs(lpOutputString, stdout);
}

//=====================================================================================================================================================================================================
//=====================================================================================================================================================================================================
template<typename _Ty> inline int __vsprintf_s(_Ty *buffer, size_t numberOfElements, const _Ty *format, va_list argptr)
{
	return 0;
}

//=====================================================================================================================================================================================================
//=====================================================================================================================================================================================================
template<> inline int __vsprintf_s<char>(char *buffer, size_t numberOfElements, const char *format, va_list argptr)
{
	return vsprintf_s(buffer, numberOfElements, format, argptr);
}

//=====================================================================================================================================================================================================
//=====================================================================================================================================================================================================
template<> inline int __vsprintf_s<wchar_t>(wchar_t *buffer, size_t numberOfElements, const wchar_t *format, va_list argptr)
{
	return vswprintf_s(buffer, numberOfElements, format, argptr);
}

//=====================================================================================================================================================================================================
//=====================================================================================================================================================================================================
template<typename _Ty> size_t __cdecl dprintf(const _Ty *szFormat,...)
{
	const size_t MAX_DEBUG_LINE_LEN_CHARS = 4096;
	_Ty szDebugOutput[MAX_DEBUG_LINE_LEN_CHARS];

	va_list arglist;
	va_start(arglist, szFormat);
	size_t numOutputStringElements = ARRAYSIZE(szDebugOutput);
	size_t size = __vsprintf_s(szDebugOutput, numOutputStringElements, szFormat, arglist);
	va_end(arglist);

	OutputDebugStringTemplate(szDebugOutput);

	return size;
}

//=====================================================================================================================================================================================================
//=====================================================================================================================================================================================================
template<typename _Ty> size_t __cdecl line_dprintf(const wchar_t *szFile, int line, const _Ty *szFormat,...)
{
	const size_t MAX_DEBUG_LINE_LEN_CHARS = 4096;
	_Ty szDebugOutput[MAX_DEBUG_LINE_LEN_CHARS];

	dprintf<wchar_t>(L"%s (%d): ", szFile, line);

	va_list arglist;
	va_start(arglist, szFormat);
	size_t numOutputStringElements = ARRAYSIZE(szDebugOutput);
	size_t size = __vsprintf_s(szDebugOutput, numOutputStringElements, szFormat, arglist);
	va_end(arglist);

	OutputDebugStringTemplate(szDebugOutput);

	return size;
}

//=====================================================================================================================================================================================================
//=====================================================================================================================================================================================================
#define __UNICODIFY(quote) L##quote
#define UNICODIFY(quote) __UNICODIFY(quote)

#define lprintf(...) line_dprintf(UNICODIFY(__FILE__), __LINE__, __VA_ARGS__)


#if 0
template<> int __cdecl dprintf<wchar_t>(const wchar_t *format,...)
{
	wchar_t szDebugOutput[MAX_DEBUG_LINE_LEN_CHARS];

	va_list arglist;
	va_start(arglist, szFormat);
	size_t numOutputStringElements = ARRAYSIZE(szDebugOutput);
	size_t size = vswprintf_s(szDebugOutput, numOutputStringElements, szFormat, arglist);
	va_end(arglist);

	OutputDebugStringW(szDebugOutput);
	OutputDebugStringW(L"\n");

	return size;
}

template<> int __cdecl dprintf<char>(const char *format,...)
{
	char szDebugOutput[MAX_DEBUG_LINE_LEN_CHARS];

	va_list arglist;
	va_start(arglist, szFormat);
	size_t numOutputStringElements = ARRAYSIZE(szDebugOutput);
	size_t size = vsprintf_s(szDebugOutput, numOutputStringElements, szFormat, arglist);
	va_end(arglist);

	OutputDebugStringA(szDebugOutput);
	fputs(szDebugOutput, stdout);

	return size;
}
#endif

