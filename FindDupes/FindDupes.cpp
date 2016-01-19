// FindDupes.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"

#include <utilities.h>
#include <FileOnDisk.h>
#include "Build_Increment.h"

#pragma comment(lib, "version.lib")

#define VERSION "3.62.2016.0118.0"

static const char szHelp[] =
"\n"
"Searches the current folder and all subfolders, looking for duplicate files\n"
"by building a table of every file, sorting them by size, and then examining\n"
"file contents for files that have the same size. For each file that needs to\n"
"be read in, an MD5 hash is calculated of its contents, and that hash is used\n"
"for the comparisons.\n"
"\n"
"In each folder where a file's contents needed to be hashed, a special hidden\n"
"file called \"md5cache.bin\" is created that contains the hash (and file info)\n"
"for all the files in that folder (that have been hashed). On subsequent runs,\n"
"that cached hash value is used as long as the file hasn't changed, saving\n"
"time, and allowing secondary passes to take only seconds, even in file systems\n"
"with tens of thousands of files.\n"
"\n"
"By default, the output is written to a log file at \"c:\\ProgramData\\dupes.log\".\n"
"\n"
"You can specify the \"-i\" command line option, followed by a folder, if you\n"
"want to target a specific folder for duplicate files. This will change the\n"
"output to only show files under that folder that have a duplicate outside\n"
"that folder. This is particularly useful if you want to delete a folder's\n"
"duplicates.\n\n"
"Options:\n"
"    /n          Omit logog.\n"
"    /c          Clean md5cache.bin files.\n"
"    /i folder   Specify an \"in\" folder.\n"
"    /I folder   Specify an \"in\" folder, and generate a delete script.\n\n"
"The \"in\" folder compares the contents of the current folder against the \"in\"\n"
"folder. The only duplicates shown are files under the \"in\" folder that have\n"
"a duplicate under the current folder. So, for example, if there are two matching\n"
"files under the \"in\" folder but nothing under the current folder that matches,\n"
"nothing will be displayed, and likewise, dupes under the current folder that don't\n"
"match something under the \"in\" folder will end up displaying nothing.\n"
"\n";

#define _LSTRINGIZE(x) L#x
#define LSTRINGIZE(x) _LSTRINGIZE(x)
#define _ASTRINGIZE(x) #x
#define ASTRINGIZE(x) _ASTRINGIZE(x)

#define BUILD_DATE_STRING ASTRINGIZE(BUILD_DATE)

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
		Logger::Get().printf(Logger::Level::Error, "%s", szHelp);
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

				Logger::Get().printf(Logger::Level::Ps1Script, "\tparam([switch]$DoIt)\n\n$files = @(\n");
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
				Logger::Get().printf(Logger::Level::Ps1Script, "\t\"\")\n");
				Logger::Get().printf(Logger::Level::Ps1Script, "$files = $files | ? { $_ -and (Test-Path $_ ) }\n\n");
				Logger::Get().printf(Logger::Level::Ps1Script, "function _Delete\n");
				Logger::Get().printf(Logger::Level::Ps1Script, "{\n");
				Logger::Get().printf(Logger::Level::Ps1Script, "\tparam([string]$File, [switch]$DoIt)\n");
				Logger::Get().printf(Logger::Level::Ps1Script, "\n");
				Logger::Get().printf(Logger::Level::Ps1Script, "\tif ($DoIt)\n");
				Logger::Get().printf(Logger::Level::Ps1Script, "\t{\n");
				Logger::Get().printf(Logger::Level::Ps1Script, "\t\tRemove-Item -Force -ErrorAction SilentlyContinue $File\n");
				Logger::Get().printf(Logger::Level::Ps1Script, "\t\tWrite-Host -Fore Gray $File\n");
				Logger::Get().printf(Logger::Level::Ps1Script, "\t}\n");
				Logger::Get().printf(Logger::Level::Ps1Script, "\telse\n");
				Logger::Get().printf(Logger::Level::Ps1Script, "\t{\n");
				Logger::Get().printf(Logger::Level::Ps1Script, "\t\tWrite-Host -Fore Red $File\n");
				Logger::Get().printf(Logger::Level::Ps1Script, "\t}\n");
				Logger::Get().printf(Logger::Level::Ps1Script, "\n");
				Logger::Get().printf(Logger::Level::Ps1Script, "\t$folder = gi (Split-Path $File)\n");
				Logger::Get().printf(Logger::Level::Ps1Script, "\n");
				Logger::Get().printf(Logger::Level::Ps1Script, "\twhile ($folder)\n");
				Logger::Get().printf(Logger::Level::Ps1Script, "\t{\n");
				Logger::Get().printf(Logger::Level::Ps1Script, "\t\tif ($folder.GetDirectories().Count -ne 0)\n");
				Logger::Get().printf(Logger::Level::Ps1Script, "\t\t{\n");
				Logger::Get().printf(Logger::Level::Ps1Script, "\t\t\tbreak\n");
				Logger::Get().printf(Logger::Level::Ps1Script, "\t\t}\n");
				Logger::Get().printf(Logger::Level::Ps1Script, "\n");
				Logger::Get().printf(Logger::Level::Ps1Script, "\t\t$files_exist = $false\n");
				Logger::Get().printf(Logger::Level::Ps1Script, "\t\t$exclusions = 'desktop.ini','thumbs.db','md5cache.bin','folder.bin','folder.jpg'\n");
				Logger::Get().printf(Logger::Level::Ps1Script, "\t\t$folder.GetFiles() | ? { $exclusions -notcontains $_.Name } | %% { $files_exist = $true }\n");
				Logger::Get().printf(Logger::Level::Ps1Script, "\n");
				Logger::Get().printf(Logger::Level::Ps1Script, "\t\tif ($files_exist)\n");
				Logger::Get().printf(Logger::Level::Ps1Script, "\t\t{\n");
				Logger::Get().printf(Logger::Level::Ps1Script, "\t\t\tbreak\n");
				Logger::Get().printf(Logger::Level::Ps1Script, "\t\t}\n");
				Logger::Get().printf(Logger::Level::Ps1Script, "\n");
				Logger::Get().printf(Logger::Level::Ps1Script, "\t\t# delete all exclusions\n");
				Logger::Get().printf(Logger::Level::Ps1Script, "\t\t$folder.GetFiles() | ? { $exclusions -contains $_.Name } | %% { $_.IsReadOnly = $false;$_.Delete() }\n");
				Logger::Get().printf(Logger::Level::Ps1Script, "\n");
				Logger::Get().printf(Logger::Level::Ps1Script, "\t\t# delete the folder\n");
				Logger::Get().printf(Logger::Level::Ps1Script, "\t\t$folder.Delete()\n");
				Logger::Get().printf(Logger::Level::Ps1Script, "\n");
				Logger::Get().printf(Logger::Level::Ps1Script, "\t\tWrite-Host -Fore White $folder.FullName\n");
				Logger::Get().printf(Logger::Level::Ps1Script, "\t\t$folder = $folder.Parent\n");
				Logger::Get().printf(Logger::Level::Ps1Script, "\t}\n");
				Logger::Get().printf(Logger::Level::Ps1Script, "}\n");
				Logger::Get().printf(Logger::Level::Ps1Script, "\n");
				Logger::Get().printf(Logger::Level::Ps1Script, "$files | %% -begin {$i=0} -process { Write-Progress -Activity \"Deleting \"\"$_\"\"...\" -PercentComplete (100.0 * $i / $files.Length);_Delete -DoIt:$DoIt $_;++$i }\n");
				Logger::Get().printf(Logger::Level::Ps1Script, "\nif (-not $DoIt)\n{\n\tWrite-Host -Fore White \"`nNot actually doing anything since the \"\"-DoIt\"\" option was not specified.`n\"\n}\n\n");
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




