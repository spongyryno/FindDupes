#include <memory>

//=====================================================================================================================================================================================================
// A class to manage a handle in local scope that will make sure to close itself when it's done
//=====================================================================================================================================================================================================
class AutoCloseHandle
{
public:
	AutoCloseHandle(HANDLE handle) : m_handle(handle) {}
	~AutoCloseHandle()
	{
		if (nullptr != this->m_handle)
		{
			::CloseHandle(this->m_handle);
			this->m_handle = nullptr;
		}
	}

	operator HANDLE() const
	{
		return this->m_handle;
	}

	HANDLE &Get()
	{
		return this->m_handle;
	}

private:
	HANDLE	m_handle;
};

//=====================================================================================================================================================================================================
//=====================================================================================================================================================================================================
inline bool AreFilesHardLinkedToEachOther(TCHAR *file1, TCHAR *file2)
{
	if (0 == _tcscmp(file1, file2))
	{
		return false;
	}

	AutoCloseHandle	hfile1 = CreateFile(file1, 0, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
	AutoCloseHandle	hfile2 = CreateFile(file2, 0, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);

	if (hfile1 && hfile2)
	{
		BY_HANDLE_FILE_INFORMATION info1 = {0};
		BY_HANDLE_FILE_INFORMATION info2 = {0};

		GetFileInformationByHandle(hfile1, &info1);
		GetFileInformationByHandle(hfile2, &info2);

		if ((info1.nNumberOfLinks < 2) || (info2.nNumberOfLinks < 2))
		{
			return false;
		}

		return ((info1.nFileIndexHigh == info2.nFileIndexHigh) && (info1.nFileIndexLow == info2.nFileIndexLow));

		__nop();
	}

	return false;
}

//=====================================================================================================================================================================================================
//=====================================================================================================================================================================================================
inline unsigned long long MAKE_LONGLONG(const DWORD &hi, const DWORD &lo)
{
	return static_cast<unsigned long long>(hi) << 32ll | static_cast<unsigned long long>(lo);
}

//=====================================================================================================================================================================================================
//=====================================================================================================================================================================================================
inline int GetHardLinkCount(const char *pszFileName, unsigned long long *pszFileIndex=nullptr)
{
	AutoCloseHandle	hfile = CreateFileA(pszFileName, 0, 0, nullptr, OPEN_EXISTING, 0, nullptr);

	if ((hfile != nullptr) && (hfile != INVALID_HANDLE_VALUE))
	{
		BY_HANDLE_FILE_INFORMATION info = {0};

		GetFileInformationByHandle(hfile, &info);

		if (pszFileIndex)
		{
			*pszFileIndex = MAKE_LONGLONG(info.nFileIndexHigh, info.nFileIndexLow);
		}

		return info.nNumberOfLinks;
	}

	return 0;
}

//=====================================================================================================================================================================================================
//=====================================================================================================================================================================================================
inline std::vector<std::wstring> GetAllHardLinksW(const wchar_t *pszFileName)
{
	std::vector<std::wstring> results;

	HANDLE hFile = CreateFileW(pszFileName, 0, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);

	if (hFile && (hFile != INVALID_HANDLE_VALUE))
	{
		BY_HANDLE_FILE_INFORMATION info = {0};

		GetFileInformationByHandle(hFile, &info);

		if (info.nNumberOfLinks > 1)
		{
			const DWORD dwStringLengthBufferSizeInCharacters = MAX_PATH * 4;
			wchar_t szHardLinkFileName[dwStringLengthBufferSizeInCharacters + 1];
			DWORD dwStringLength;
			const size_t drivesize = ARRAYSIZE("h:") - 1;

			// copy over the drive...
			wcsncpy_s(szHardLinkFileName, ARRAYSIZE(szHardLinkFileName), pszFileName, drivesize);

			dwStringLength = ARRAYSIZE(szHardLinkFileName) - drivesize;
			wchar_t *szHardLinkSubStringFileName = &szHardLinkFileName[drivesize];

			HANDLE hFind = FindFirstFileNameW(pszFileName, 0, &dwStringLength, szHardLinkSubStringFileName);

			if (INVALID_HANDLE_VALUE != hFind)
			{
				results.push_back(szHardLinkFileName);
				dwStringLength = ARRAYSIZE(szHardLinkFileName) - drivesize;

				while (FindNextFileNameW(hFind, &dwStringLength, szHardLinkSubStringFileName))
				{
					results.push_back(szHardLinkFileName);
					dwStringLength = ARRAYSIZE(szHardLinkFileName) - drivesize;
				}

				FindClose(hFind);
			}
		}

		CloseHandle(hFile);
	}

	return results;
}



//=====================================================================================================================================================================================================
//=====================================================================================================================================================================================================
inline std::string UnicodeToAnsi(const std::wstring &src)
{
	size_t s = src.size();

	int size_needed = WideCharToMultiByte(CP_ACP, 0, src.c_str(), -1, nullptr, 0, nullptr, nullptr);
	char *pstring = (char *)_alloca(size_needed * sizeof(char));
	WideCharToMultiByte(CP_ACP, 0, src.c_str(), -1, pstring, size_needed, nullptr, nullptr);
	return std::string(pstring);
}

//=====================================================================================================================================================================================================
//=====================================================================================================================================================================================================
inline std::wstring AnsiToUnicode(const std::string &src)
{
	size_t s = src.size();

	int size_needed = MultiByteToWideChar(CP_ACP, 0, src.c_str(), -1, nullptr, 0);
	wchar_t *pwstring = (wchar_t *)_alloca(size_needed * sizeof(wchar_t));
	MultiByteToWideChar(CP_ACP, 0, src.c_str(), -1, pwstring, size_needed);
	return std::wstring(pwstring);
}

//=====================================================================================================================================================================================================
//=====================================================================================================================================================================================================
inline std::vector<std::string> GetAllHardLinksA(const char *pszFileName)
{
	std::vector<std::string> results;
	std::vector<std::wstring> wresults = GetAllHardLinksW(AnsiToUnicode(std::string(pszFileName)).c_str());

	// allocate enough space
	results.reserve(wresults.size());

	for (auto &i : wresults)
	{
		results.push_back(UnicodeToAnsi(i));
	}

	return results;
}






