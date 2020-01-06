// FindDupes.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"

#include <utilities.h>
#include <FileOnDisk.h>
#include <HardLink.h>
#include "Build_Increment.h"
#include "Resource.h"

#pragma comment(lib, "version.lib")

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
	std::wstring _szDupesLogFile;
	//path foo;
	//std::experimental::filesystem::v1::path foo {R"(c:\foo.txt)"};

	char szRootFolder[fileMax];
	char szInFolder[fileMax];
	char szSyncFolderLeft[fileMax];
	char szSyncFolderRght[fileMax];


	// options
	bool trim = false;
	bool infile = false;
	bool logo = true;
	bool showHelp = false;
	bool includeDeleteScript = false;
	bool cleanCacheFiles = false;
	bool verbose = false;
	bool generateHashForAllFiles = false;
	bool syncFolders = false;

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
	for (int i = 1; i < argc; ++i)
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

				++i;
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
			else if (L'v' == argv[i][1])
			{
				verbose = true;
				Logger::Get().SetOutLevel(Logger::Level::All);
			}
			else if (L'a' == argv[i][1])
			{
				generateHashForAllFiles = true;
			}
			else if (L's' == argv[i][1])
			{
				if (argc < i + 2)
				{
					Logger::Get().printf(Logger::Level::Error, "Error: missing args\n");
					return -1;
				}

				++i;
				WideCharToMultiByte(CP_ACP, 0, argv[i], -1, szSyncFolderLeft, sizeof(szSyncFolderLeft), nullptr, nullptr);

				++i;
				WideCharToMultiByte(CP_ACP, 0, argv[i], -1, szSyncFolderRght, sizeof(szSyncFolderRght), nullptr, nullptr);
				syncFolders = true;

				if (!RelativeToFullpath(szSyncFolderLeft, ARRAYSIZE(szSyncFolderLeft)))
				{
					Logger::Get().printf(Logger::Level::Error, "Error: could not resolve \"%s\"\n", szSyncFolderLeft);
					return -1;
				}

				if (!RelativeToFullpath(szSyncFolderRght, ARRAYSIZE(szSyncFolderRght)))
				{
					Logger::Get().printf(Logger::Level::Error, "Error: could not resolve \"%s\"\n", szSyncFolderRght);
					return -1;
				}

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
		Logger::Get().printf(Logger::Level::Error, "SpongySoft FindDupes version %S (" __TIMESTAMP__ ") (" BUILD_DATE_STRING ") (Compiled for %d-bit)\nCopyright (c) 2000-2019 SpongySoft.\nFor more information, visit http://www.spongysoft.com\n", version.c_str(), sizeof(void *)* 8);
	}

	if (showHelp)
	{
		DWORD dwLen = 0;
		Logger::Get().puts(Logger::Level::Error, reinterpret_cast<char *>(LoadResource(GetModuleHandle(nullptr), ID_BINARY, ID_USAGE, &dwLen)), dwLen);
		return -1;
	}
	else if (cleanCacheFiles)
	{
		verboseprintf("Cleaning cache files...\n");
		CleanCacheFiles(szRootFolder);
	}
	else if (generateHashForAllFiles)
	{
		verboseprintf("Generating hash for ALL files...\n");
		const size_t itemsSizeReserve = 500000;
		const size_t avgStringSizeToReserve = 128;

		// create the files list
		FileOnDiskSet files;

		files.Items.reserve(itemsSizeReserve);
		files.Strings.reserve(itemsSizeReserve * avgStringSizeToReserve);

		{
			TimeThis t("Read the directory structure");
			files.QueryFileSystem(szRootFolder);
			files.CheckStrings();
			Logger::Get().printf(Logger::Level::Debug, "There are %s files in the directory structure.\n", comma(files.Items.size()));
		}

		// make sure all files that have the same size have updated hashes, and save the hash caches if necessary
		{
			TimeThis t("To update hashes");
			files.UpdateHashedFiles(true, verbose);
		}
	}
	else if (syncFolders)
	{
		verboseprintf("Syncing folders \"%s\" and \"%s\".\n", szSyncFolderLeft, szSyncFolderRght);
		const size_t itemsSizeReserve = 500000;
		const size_t avgStringSizeToReserve = 128;

		// create the files list
		FileOnDiskSet left;
		FileOnDiskSet rght;

		left.Items.reserve(itemsSizeReserve);
		left.Strings.reserve(itemsSizeReserve * avgStringSizeToReserve);

		rght.Items.reserve(itemsSizeReserve);
		rght.Strings.reserve(itemsSizeReserve * avgStringSizeToReserve);

		{
			TimeThis t("Read the first directory structure");
			verboseprintf("Reading the first directory structure...\n");
			left.QueryFileSystem(szSyncFolderLeft, true);
			left.CheckStrings();
			Logger::Get().printf(Logger::Level::Debug, "There are %s files in the directory structure.\n", comma(left.Items.size()));
		}

		{
			TimeThis t("Read the second directory structure");
			verboseprintf("Reading the second directory structure...\n");
			rght.QueryFileSystem(szSyncFolderRght, true);
			rght.CheckStrings();
			Logger::Get().printf(Logger::Level::Debug, "There are %s files in the directory structure.\n", comma(rght.Items.size()));
		}

		std::unordered_map<Path, FileOnDisk *> umap;
		umap.reserve(left.Items.size());

		for (auto &item : left.Items)
		{
			auto lname = left.GetFileName(item);
			auto lpath = left.GetFilePath(item);
			auto lsubp = left.GetSubPathName(item);

			auto subpathname = lsubp;
			umap[subpathname] = &item;
		}

		// validation
		for (auto &i : umap)
		{
			auto key = i.first;
			auto &val = *(i.second);

			auto lname = left.GetFileName(val);
			auto lpath = left.GetFilePath(val);
			auto lsubp = left.GetSubPathName(val);

			__nop();
		}

		size_t leftNumFiles = left.Items.size();
		size_t rghtNumFiles = rght.Items.size();
		size_t bothNumFiles = 0;

		for (auto &item : rght.Items)
		{
			auto rname = rght.GetFileName(item);
			auto rpath = rght.GetFilePath(item);
			auto rsubp = rght.GetSubPathName(item);

			//if (0 == strcmp(rsubp, R"(Software\TurboTax\2013\TurboTax 2013\Runtime\license.rtf)"))
			if (0 == strcmp(rsubp, R"(Runtime\license.rtf)"))
			{
				__nop();
			}
			else
			{
				__nop();
			}

			auto subpathname = rsubp;
			auto leftitemref = umap.find(subpathname);
			if (leftitemref != umap.end())
			{
				++bothNumFiles;

				auto &leftitem = *(leftitemref->second);

				auto lname = left.GetFileName(leftitem);
				auto lpath = left.GetFilePath(leftitem);
				auto lsubp = left.GetSubPathName(leftitem);

				// found!
				if (leftitem.Size == item.Size)
				{
					// same size...
					auto lefttime_cached = FileTimeToUInt64(leftitem.Time);
					auto rghttime_cached = FileTimeToUInt64(item.Time);

#ifdef _DEBUG
//#define VALIDATE_TIMESTAMPS
#endif
#ifdef VALIDATE_TIMESTAMPS
					auto lefttime_actual = FileTimeToUInt64(GetFileTimeStamp(lpath));
					auto rghttime_actual = FileTimeToUInt64(GetFileTimeStamp(rpath));

					if (lefttime_cached != lefttime_actual)
					{
						Logger::Get().printf(Logger::Level::Error, "Timestamp mismatch between cache file and actual file: \"%s\".\n", lpath);
					}

					if (rghttime_cached != rghttime_actual)
					{
						Logger::Get().printf(Logger::Level::Error, "Timestamp mismatch between cache file and actual file: \"%s\".\n", rpath);
					}
#endif

					if (leftitem.Hashed)
					{
						if (item.Hashed)
						{
							// both hashed...
							if (leftitem.Hash == item.Hash)
							{
								// same hash!!
								if (lefttime_cached > rghttime_cached)
								{
									// same hash, right side needs a timestamp update
									__nop();
								}
								else if (lefttime_cached < rghttime_cached)
								{
									// same hash, left side needs a timestamp update
									__nop();
								}
								else
								{
									// same hash, nothing to do
									__nop();
								}
							}
							else
							{
								// different hashes and timestamps, but the same size!!
								__nop();
							}
						}
						else
						{
							// check the ACTUAL timestamps on the files
							// same size, left item is hashed, right item is not
							if (lefttime_cached > rghttime_cached)
							{
								__nop();
							}
							else if (lefttime_cached < rghttime_cached)
							{
								__nop();
							}
							else
							{
								Logger::Get().printf(Logger::Level::Info, "Copying hash from:\n    \"%s\" to\n    \"%s\"\n", lpath, rpath);

								item.Hash = leftitem.Hash;
								item.Hashed = true;
								rght.UpdateFile(item);
							}
						}
					}
					else
					{
						if (item.Hashed)
						{
							if (lefttime_cached > rghttime_cached)
							{
								__nop();
							}
							else if (lefttime_cached < rghttime_cached)
							{
								__nop();
							}
							else
							{
								Logger::Get().printf(Logger::Level::Info, "Copying hash from:\n    \"%s\" to\n    \"%s\"\n", rpath, lpath);

								leftitem.Hash = item.Hash;
								leftitem.Hashed = true;
								left.UpdateFile(leftitem);
							}
						}
						else
						{
							// same size, neither is hashed
							__nop();
						}
					}
				}
				else
				{
					// different sizes
					__nop();
				}
			}
			else
			{
				// file on right size doesn't exist in the left
				__nop();
			}
		}

		Logger::Get().printf(Logger::Level::Info, "Number of files on left:       %15s\n", comma(leftNumFiles));
		Logger::Get().printf(Logger::Level::Info, "Number of files on right:      %15s\n", comma(rghtNumFiles));
		Logger::Get().printf(Logger::Level::Info, "Number of files only on left:  %15s\n", comma(leftNumFiles-bothNumFiles));
		Logger::Get().printf(Logger::Level::Info, "Number of files only on right: %15s\n", comma(rghtNumFiles-bothNumFiles));
		Logger::Get().printf(Logger::Level::Info, "Number of files in both:       %15s\n", comma(bothNumFiles));
	}
	else
	{
		// convert the infile to the full path name
		if (infile)
		{
			if (includeDeleteScript)
			{
				verboseprintf("Finding dupes against an \"in\" file with a delete script...\n");

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
			else
			{
				verboseprintf("Finding dupes against an \"in\" file (no delete script)...\n");
			}

			if (!RelativeToFullpath(szInFolder, ARRAYSIZE(szInFolder)))
			{
				Logger::Get().printf(Logger::Level::Error, "Error: cannot get include folder.\n");
				return -1;
			}
		}
		else
		{
			verboseprintf("Finding all dupes (no \"in\" file)...\n");
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
			verboseprintf("Reading the directory structure...\n");
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
				verboseprintf("Reading the directory structure of the \"in\" folder...\n");
				infiles.QueryFileSystem(szInFolder);
				Logger::Get().printf(Logger::Level::Debug, "There are %s files in the \"in\" directory structure.\n", comma(infiles.Items.size()));
			}

			// remove all "infiles" from "files" that may exist
			{
				TimeThis t("Remove \"in\" files from the rest");
				verboseprintf("Removing infiles from files...\n");
				files.RemoveSetFromSet(infiles);
			}

			// go through both lists and make sure that anything that may need a hash has one
			{
				FileOnDiskSet allFiles;

				{
					TimeThis t("Merge all files into one uber set.");
					verboseprintf("Merging all files into one uber set...\n");
					allFiles.MergeFrom(files);
					files.CheckStrings();
					allFiles.CheckStrings();
					allFiles.MergeFrom(infiles);
				}

				{
					TimeThis t("Hash necessary files.");
					verboseprintf("Hashing necessary files...\n");
					allFiles.UpdateHashedFiles(false, verbose);
				}

				{
					TimeThis t("Apply hashes from uber set to individual sets.");
					verboseprintf("Applying hashes from uber set to individual sets...\n");
					files.ApplyHashFrom(allFiles);
					infiles.CheckStrings();
					infiles.ApplyHashFrom(allFiles);
				}

				// we're done with all files
				{
					TimeThis t("Clean up uber set.");
					verboseprintf("Cleaning up uber set...\n");
					allFiles.Items.clear();
					allFiles.Strings.clear();
				}
			}

			// make a multimap of all files with a given size in the "files" list
			std::unordered_map<long long, std::vector<size_t>> umap;

			{
				size_t index = 0;
				for (auto &file : files.Items)
				{
					umap[file.Size].push_back(index);
					++index;
				}
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

			for (auto &infile : infiles.Items)
			{
				if (umap.find(infile.Size) != umap.end())
				{
					// there are files that match the infile's size. See if any of them have the same hash
					bool hashMatch = false;

					std::vector<size_t> &indices = umap[infile.Size];

					for (auto &index : indices)
					{
						const FileOnDisk &file = files.Items[index];
						if (infile.Hash == file.Hash)
						{
							hashMatch = true;
						}
					}

					if (hashMatch)
					{
						if (includeDeleteScript)
						{
							Logger::Get().printf(Logger::Level::CmdScript, "del /F /A \"%s\"\n", infiles.GetFilePath(infile));
							Logger::Get().printf(Logger::Level::Ps1Script, "\t\"%s\",\n", infiles.GetFilePath(infile));
						}

						Logger::Get().printf(Logger::Level::Dupes, "====================================================================================================\n");
						Logger::Get().printf(Logger::Level::Dupes, "        %20s %s \"%s\"\n", comma(infile.Size), infile.Hash.ToString(), infiles.GetFilePath(infile));

						++duplicateFiles;
						duplicateBytes += infile.Size;

						for (auto &index : indices)
						{
							const FileOnDisk &file = files.Items[index];

							if (infile.Hash == file.Hash)
							{
								Logger::Get().printf(Logger::Level::Dupes, "        %20s %s \"%s\"\n", comma(file.Size), file.Hash.ToString(), files.GetFilePath(file));
							}
						}
					}
				}
			}

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
				verboseprintf("Updating hashes...\n");
				files.UpdateHashedFiles(false, verbose);
			}

			// now, find the dupes
			{
				TimeThis t("To find dupes");

				// go through each item. we cannot do a "for each" here, since we may skip some
				if (!files.Items.empty())
				{
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
							++j;
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
						for (size_t i0 = i; i0 < j; ++i0)
						{
							diff.insert(files.Items[i0]);
						}

						// now, loop until the "diff" set is empty
						do
						{
							// get the hash of the first file in the set, which is the item we're going to compare with
							auto &compareitem = *diff.begin();

							// put all the files in "diff" that match the hash into the "same" bucket
							for (auto &file : diff)
							{
								if (compareitem.Hash == file.Hash)
								{
									same.insert(file);
								}
							}

							// remove the items that are in the "same" bucket from the "diff" bucket
							for (auto &file : same)
							{
								diff.erase(file);
							}

							// we will always have the "compareitem", but do we have more than just that one?
							if (same.size()>1)
							{
								duplicateFiles += same.size() - 1;
								duplicateBytes += (same.size() - 1) * (same.begin()->Size);

								long hardLinkChar = static_cast<long>('a');
								std::unordered_map<unsigned long long, long> hardLinkMap;

								for (auto &file : same)
								{
									auto filePath = files.GetFilePath(file);
									file.nNumberOfLinks = GetHardLinkCount(filePath, &file.nFileIndex);

									if (hardLinkMap.find(file.nFileIndex) == hardLinkMap.end())
									{
										hardLinkMap[file.nFileIndex] = hardLinkChar;
										++hardLinkChar;
									}
								}

								Logger::Get().printf(Logger::Level::Dupes, "    ================================================================================================\n");

								for (auto &file : same)
								{
									auto filePath = files.GetFilePath(file);
									long hardLinkCharLong = hardLinkMap[file.nFileIndex];
									char hardLinkChar = hardLinkCharLong <= static_cast<long>('z') ? static_cast<char>(hardLinkCharLong) : '*';
									Logger::Get().printf(Logger::Level::Dupes, "        %20s %s (%d,%c) \"%s\"\n", comma(file.Size), file.Hash.ToString(), file.nNumberOfLinks, hardLinkChar, filePath);
								}
							}

							same.clear();

						} while (diff.size() > 0);


						i = j;
					}
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

#ifdef _DEBUG
	if (IsDebuggerPresent())
	{
		while (_kbhit()) _getch();
		_getch();
	}
#endif

	return 0;
}




