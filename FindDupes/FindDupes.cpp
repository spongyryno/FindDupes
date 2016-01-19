// FindDupes.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"

#include <utilities.h>
#include <FileOnDisk.h>
#include "Build_Increment.h"
#include "Resource.h"

#pragma comment(lib, "version.lib")

#define VERSION "3.63.2016.0119.0"

#define _LSTRINGIZE(x) L#x
#define LSTRINGIZE(x) _LSTRINGIZE(x)
#define _ASTRINGIZE(x) #x
#define ASTRINGIZE(x) _ASTRINGIZE(x)

#define BUILD_DATE_STRING ASTRINGIZE(BUILD_DATE)

//========================================================================
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
//========================================================================
static void *LoadResource(HINSTANCE hInst, LPCWSTR szType, LPCWSTR szItem, DWORD *pdwSize=NULL)
{
	HRSRC	hRsrcInfo;
	HGLOBAL hGlobal;
	LPVOID	pR;
	DWORD	dwSize;

	if (!(hRsrcInfo = FindResource( hInst, szItem, szType )))
	{
		return NULL;
	}

	dwSize=SizeofResource(hInst, hRsrcInfo);

	if (!(hGlobal = LoadResource(hInst, hRsrcInfo)))
	{
		return NULL;
	}

	if (!(pR=LockResource(hGlobal)))
	{
		return NULL;
	}

	if (pdwSize)
		*pdwSize = dwSize;

	return pR;
}

//========================================================================
//========================================================================
static void *LoadResource(HINSTANCE hInst, size_t szType, size_t szItem, DWORD *pdwSize=NULL)
{
	return LoadResource(hInst, reinterpret_cast<LPCWSTR>(szType), reinterpret_cast<LPCWSTR>(szItem), pdwSize);
}


//==================================================================================================
//==================================================================================================
std::wstring GetLocalBinaryVersionString(void)
{
	TCHAR szModuleFileName[MAX_PATH];
	VS_FIXEDFILEINFO *	verinfo;

	GetModuleFileName(nullptr, szModuleFileName, ARRAYSIZE(szModuleFileName));

	DWORD dwSize = GetFileVersionInfoSize(szModuleFileName, 0);
	if (!dwSize)
	{
		return nullptr;
	}

	std::vector<BYTE> buffer = std::vector<BYTE>(dwSize);

	if (!GetFileVersionInfo(szModuleFileName, 0, dwSize, &buffer[0]))
	{
		return nullptr;
	}

	if (!VerQueryValue(&buffer[0], _T("\\"), (void **)&verinfo, (UINT *)&dwSize))
	{
		return nullptr;
	}

	return
		std::to_wstring(HIWORD(verinfo->dwFileVersionMS)) + L"." +
		std::to_wstring(LOWORD(verinfo->dwFileVersionMS)) + L"." +
		std::to_wstring(HIWORD(verinfo->dwFileVersionLS)) + L"." +
		std::to_wstring(LOWORD(verinfo->dwFileVersionLS));
}

//==================================================================================================
// main program
//==================================================================================================
int _tmain(int argc, _TCHAR* argv[])
{
#ifdef _DEBUG
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif
	const size_t fileMax = 16384;

	wchar_t szDupesLogFile[fileMax];
	wchar_t szDupesCmdFile[fileMax];
	wchar_t szDupesPs1File[fileMax];
	char szRootFolder[fileMax];
	char szInFolder[fileMax];

	// options
	bool trim = false;
	bool infile = false;
	bool logo = true;
	bool showHelp = false;
	bool includeDeleteScript = false;
	bool cleanCacheFiles = false;

	wchar_t szAppData[MAX_PATH];
	HRESULT hr;

	// get the default location of the log file
	DWORD dwPathSize = GetEnvironmentVariable(L"temp", szAppData, ARRAYSIZE(szAppData));
	if ((0 == dwPathSize) || (dwPathSize > ARRAYSIZE(szAppData)))
	{
		hr = SHGetFolderPath(nullptr, CSIDL_COMMON_APPDATA, nullptr, SHGFP_TYPE_CURRENT, szAppData);
		if (FAILED(hr))
		{
			fprintf(stderr, "Error: 0x%08X getting common appdata folder\n", hr);
			return -1;
		}
	}

	wcscpy_s(szDupesLogFile, szAppData);
	wcscat_s(szDupesLogFile, L"\\dupes.log");
	wcscpy_s(szDupesCmdFile, szAppData);
	wcscat_s(szDupesCmdFile, L"\\dupes.cmd");
	wcscpy_s(szDupesPs1File, szAppData);
	wcscat_s(szDupesPs1File, L"\\dupes.ps1");

	if (!Logger::Get().Open(szDupesLogFile))
	{
		fprintf(stderr, "Error: %d Could not open log file.\n", GetLastError());
		return -1;
	}
	Logger::Get().Reset();

	// get the default location of the root folder
	GetCurrentDirectoryA(ARRAYSIZE(szRootFolder), szRootFolder);

	// remove any trailing backslash
	if (szRootFolder[strlen(szRootFolder) - 1] == '\\')
	{
		szRootFolder[strlen(szRootFolder) - 1] = 0;
	}

	// make sure that under a debugger, we have "_NO_DEBUG_HEAP" defined
	static bool allowDebugHeap = false;
	if (!allowDebugHeap)
	{
		if (IsDebuggerPresent())
		{
			if (0 == GetEnvironmentVariable(L"_NO_DEBUG_HEAP", nullptr, 0))
			{
				Logger::Get().printf(Logger::Level::Error, "Error: Debug heap not disabled (_NO_DEBUG_HEAP), and we're running under a debugger\n");
				return -1;
			}
		}
	}

	//=============================================================================================
	// parse command-line options
	//=============================================================================================
	for (int i = 1; i < argc; i++)
	{
		if ((L'-' == argv[i][0]) || (L'/' == argv[i][0]))
		{
			if ((L'i' == argv[i][1]) || (L'I' == argv[i][1]))
			{
				includeDeleteScript = (L'I' == argv[i][1]);

				if (argc < i + 1)
				{
					Logger::Get().printf(Logger::Level::Error, "Error: missing arg\n");
					return -1;
				}

				i++;
				WideCharToMultiByte(CP_ACP, 0, argv[i], -1, szInFolder, sizeof(szInFolder), nullptr, nullptr);
				infile = true;
			}
			else if (L'n' == argv[i][1])
			{
				logo = false;
			}
			else if ((L'?' == argv[i][1]) || (L'h' == tolower(argv[i][1])))
			{
				showHelp = true;
			}
			else if (L'c' == argv[i][1])
			{
				cleanCacheFiles = true;
			}
			else
			{
				Logger::Get().printf(Logger::Level::Error, "Unknown option: \"%s\"\n", argv[i]);
				showHelp = true;
			}
		}
		else
		{
			Logger::Get().printf(Logger::Level::Error, "Unknown option: \"%s\"\n", argv[i]);
			showHelp = true;
		}
	}
	//=============================================================================================
	//=============================================================================================


	if (logo)
	{
		std::wstring version = GetLocalBinaryVersionString();
		Logger::Get().printf(Logger::Level::Error, "SpongySoft FindDupes version %S (" __TIMESTAMP__ ") (" BUILD_DATE_STRING ") (Compiled for %d-bit)\nCopyright (c) 2000-2014 SpongySoft.\nFor more information, visit http://www.spongysoft.com\n", version.c_str(), sizeof(void *)* 8);
	}

	if (showHelp)
	{
		DWORD dwLen = 0;
		Logger::Get().puts(Logger::Level::Error, reinterpret_cast<char *>(LoadResource(GetModuleHandle(nullptr), ID_BINARY, ID_USAGE, &dwLen)), dwLen);
		return -1;
	}
	else if (cleanCacheFiles)
	{
		CleanCacheFiles(szRootFolder);
	}
	else
	{
		// convert the infile to the full path name
		if (infile)
		{
			if (includeDeleteScript)
			{
				if (!Logger::Get().OpenCmdScript(szDupesCmdFile))
				{
					fprintf(stderr, "Error: %d Could not open log file.\n", GetLastError());
					return -1;
				}

				if (!Logger::Get().OpenPs1Script(szDupesPs1File))
				{
					fprintf(stderr, "Error: %d Could not open log file.\n", GetLastError());
					return -1;
				}

				DWORD dwLen = 0;
				Logger::Get().puts(Logger::Level::Ps1Script, reinterpret_cast<char *>(LoadResource(GetModuleHandle(nullptr), ID_BINARY, ID_PSHEADER, &dwLen)), dwLen);
			}

			if (!RelativeToFullpath(szInFolder, ARRAYSIZE(szInFolder)))
			{
				Logger::Get().printf(Logger::Level::Error, "Error: cannot get include folder.\n");
				return -1;
			}
		}

		const size_t itemsSizeReserve = 500000;
		const size_t avgStringSizeToReserve = 128;

		// create the files list
		FileOnDiskSet files;

		files.Items.reserve(itemsSizeReserve);
		files.Strings.reserve(itemsSizeReserve * avgStringSizeToReserve);

		// read the files on disk
		{
			TimeThis t("Read the directory structure");
			files.QueryFileSystem(szRootFolder);
			files.CheckStrings();
			Logger::Get().printf(Logger::Level::Debug, "There are %s files in the directory structure.\n", comma(files.Items.size()));
		}

		// log header
		{
			SYSTEMTIME st;
			GetLocalTime(&st);
			Logger::Get().printf(Logger::Level::Debug, "====================================================================================================\n");
			Logger::Get().printf(Logger::Level::Debug, "Ran on %02d/%02d/%04d at %02d:%02d:%02d.%03d\n", st.wMonth, st.wDay, st.wYear, st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);
		}

		long long duplicateBytes = 0;
		long long duplicateFiles = 0;

		if (infile)
		{
			FileOnDiskSet infiles;
			infiles.Items.reserve(itemsSizeReserve);
			infiles.Strings.reserve(itemsSizeReserve * avgStringSizeToReserve);

			// read the disk
			{
				TimeThis t("Read the \"in\" directory structure");
				infiles.QueryFileSystem(szInFolder);
				Logger::Get().printf(Logger::Level::Debug, "There are %s files in the \"in\" directory structure.\n", comma(infiles.Items.size()));
			}

			// remove all "infiles" from "files" that may exist
			{
				TimeThis t("Remove \"in\" files from the rest");
				files.RemoveSetFromSet(infiles);
			}

			// go through both lists and make sure that anything that may need a hash has one
			{
				FileOnDiskSet allFiles;

				{
					TimeThis t("Merge all files into one uber set.");
					allFiles.MergeFrom(files);
					files.CheckStrings();
					allFiles.CheckStrings();
					allFiles.MergeFrom(infiles);
				}

				{
					TimeThis t("Hash necessary files.");
					allFiles.UpdateHashedFiles();
				}

				{
					TimeThis t("Apply hashes from uber set to individual sets.");
					files.ApplyHashFrom(allFiles);
					infiles.CheckStrings();
					infiles.ApplyHashFrom(allFiles);
				}

				// we're done with all files
				{
					TimeThis t("Cleanup uber set.");
					allFiles.Items.clear();
					allFiles.Strings.clear();
				}
			}

			// make a multimap of all files with a given size in the "files" list
			std::unordered_map<long long, std::vector<size_t>> umap;

			{
				size_t index = 0;
				for_each(files.Items.begin(), files.Items.end(), [&](const FileOnDisk &file)
				{
					umap[file.Size].push_back(index);
					index++;
				});
			}

			// sort the "infiles" on size
			std::sort(infiles.Items.begin(), infiles.Items.end(), [&](FileOnDisk const &left, FileOnDisk const &right)
			{
				if (left.Size == right.Size)
				{
					return (_stricmp(infiles.GetFilePath(left), infiles.GetFilePath(right)) < 0);
				}

				return left.Size > right.Size;
			});

			for_each(infiles.Items.begin(), infiles.Items.end(), [&](const FileOnDisk &infile)
			{
				if (umap.find(infile.Size) != umap.end())
				{
					// there are files that match the infile's size. See if any of them have the same hash
					bool hashMatch = false;

					std::vector<size_t> &indices = umap[infile.Size];

					for_each(indices.begin(), indices.end(), [&](size_t index)
					{
						const FileOnDisk &file = files.Items[index];
						if (0 == memcmp(infile.Hash, file.Hash, sizeof(infile.Hash)))
						{
							hashMatch = true;
						}
					});

					if (hashMatch)
					{
						if (includeDeleteScript)
						{
							Logger::Get().printf(Logger::Level::CmdScript, "del /F /A \"%s\"\n", infiles.GetFilePath(infile));
							Logger::Get().printf(Logger::Level::Ps1Script, "\t\"%s\",\n", infiles.GetFilePath(infile));
						}

						Logger::Get().printf(Logger::Level::Dupes, "====================================================================================================\n");
						Logger::Get().printf(Logger::Level::Dupes, "        %20s %s \"%s\"\n", comma(infile.Size), Md5HashToString(infile.Hash), infiles.GetFilePath(infile));

						duplicateFiles ++;
						duplicateBytes += infile.Size;

						for_each(indices.begin(), indices.end(), [&](size_t index)
						{
							const FileOnDisk &file = files.Items[index];

							if (0 == memcmp(infile.Hash, file.Hash, sizeof(infile.Hash)))
							{
								Logger::Get().printf(Logger::Level::Dupes, "        %20s %s \"%s\"\n", comma(file.Size), Md5HashToString(file.Hash), files.GetFilePath(file));
							}
						});
					}
				}
			});

			if (includeDeleteScript)
			{
				DWORD dwLen = 0;
				Logger::Get().puts(Logger::Level::Ps1Script, reinterpret_cast<char *>(LoadResource(GetModuleHandle(nullptr), ID_BINARY, ID_PSFOOTER, &dwLen)), dwLen);
			}
		}
		else
		{
			// make sure all files that have the same size have updated hashes, and save the hash caches if necessary
			{
				TimeThis t("To update hashes");
				files.UpdateHashedFiles();
			}

			// now, find the dupes
			{
				TimeThis t("To find dupes");

				// go through each item. we cannot do a "for_each" here, since we may skip some
				for (size_t i = 0; i<files.Items.size() - 1;)
				{
					FileOnDisk &file = files.Items[i];

					// quit when we reach zero-byte sized files
					if (file.Size == 0)
					{
						break;
					}

					// now, loop through every file that is the same size
					size_t j = i + 1;
					while (j<files.Items.size() && (files.Items[j].Size == file.Size))
					{
						j++;
					}

					if ((j - i) == 1)
					{
						// this file has no file-size match, so it can't be a duplicate, so continue on
						++i;
						continue;
					}

					// now each file from i..j-1 are all the same size
					auto PathCompare = [&](const FileOnDisk&left, const FileOnDisk&right)->bool
					{
						return (0 > _stricmp(files.GetFilePath(left), files.GetFilePath(right)));
					};

					std::set<FileOnDisk, std::function<bool(const FileOnDisk&left, const FileOnDisk&right)> > same(PathCompare);
					std::set<FileOnDisk, std::function<bool(const FileOnDisk&left, const FileOnDisk&right)> > diff(PathCompare);

					// leave the "same" set empty, but put everything in the "diff" set to start with
					for (size_t i0 = i; i0 < j; i0++)
					{
						diff.insert(files.Items[i0]);
					}

					// now, loop until the "diff" set is empty
					do
					{
						const unsigned char *baseHash = &(*diff.begin()).Hash[0];

						for_each(diff.begin(), diff.end(), [&](const FileOnDisk &file)
						{
							if (0 == memcmp(baseHash, file.Hash, sizeof(file.Hash)))
							{
								same.insert(file);
							}
						});

						for_each(same.begin(), same.end(), [&](const FileOnDisk &file)
						{
							diff.erase(file);
						});

						if (same.size()>1)
						{
							duplicateFiles += same.size() - 1;
							duplicateBytes += (same.size() - 1) * (same.begin()->Size);

							Logger::Get().printf(Logger::Level::Dupes, "    ================================================================================================\n");

							for_each(same.begin(), same.end(), [&](const FileOnDisk &file)
							{
								Logger::Get().printf(Logger::Level::Dupes, "        %20s %s \"%s\"\n", comma(file.Size), Md5HashToString(file.Hash), files.GetFilePath(file));
							});
						}

						same.clear();

					} while (diff.size() > 0);


					i = j;
				}
			}
		}

		Logger::Get().printf(Logger::Level::Info, "%15s Duplicate Files\n", comma(duplicateFiles));
		Logger::Get().printf(Logger::Level::Info, "%15s Duplicate Bytes\n", comma(duplicateBytes));
		Logger::Get().Close();
	}

	printf("Log file: \"%S\"\n", szDupesLogFile);
	if (includeDeleteScript)
	{
		printf("Cmd file: \"%S\"\n", szDupesCmdFile);
		printf("Ps1 file: \"%S\"\n", szDupesPs1File);
	}

	return 0;
}




