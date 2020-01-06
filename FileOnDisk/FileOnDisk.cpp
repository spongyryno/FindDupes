// FileOnDisk.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"

#include <utilities.h>
#include <FileOnDisk.h>
#include <HardLink.h>

const size_t maxString = 1024;
static const char *szFileNamesToIgnore[] = { ".","..","desktop.ini","folder.bin","folder.jpg",pszLocalCacheFileName,pszOldLocalCacheFileName };

#define NO_LONGER_NEED_TO_RENAME_OLD_CACHE_FILES

//=================================================================================================
// Internal functions
//=================================================================================================
static void ProcessFolder(const char *szName, FileOnDiskSet &files, int depth, bool clean);
static void ProcessFile(const char *szFolderName, WIN32_FIND_DATAA *pfd, FileOnDiskSet &files, int depth, std::unordered_map<Path,const Md5CacheItem *> *pumap, bool clean);
static bool CalcFileMd5Hash(const char *szFileName, Md5Hash &hash, bool verbose);
static bool GetFileMd5Hash(const char *szFileName, Md5Hash &hash, bool verbose);
static bool GetCachedHash(const char *szFileName, Md5Hash &hash, bool verbose);

#define MAX_STRING 1024

#define MAKELONGLONG(lo,hi) ((long long)(((unsigned long long)(lo)) | (((unsigned long long)(hi)) << 32)))



//=================================================================================================
//=================================================================================================
inline long long GetWin32FindDataFileSize(const WIN32_FIND_DATAA &fd)
{
	return MAKELONGLONG(fd.nFileSizeLow, fd.nFileSizeHigh);
}

//=================================================================================================
//=================================================================================================
inline std::string GetFolderWildcard(const std::string &folder)
{
	std::string wildcard{folder};
	wildcard.append(R"(\*.*)");
	return wildcard;
}

//=================================================================================================
//=================================================================================================
inline std::string CleanupOldCacheFiles(const std::string folderName)
{
	std::string newfoldercache{folderName};
	std::string	oldfoldercache{newfoldercache};

	newfoldercache.append(R"(\)");
	newfoldercache.append(pszLocalCacheFileName);

	oldfoldercache.append(R"(\)");
	oldfoldercache.append(pszOldLocalCacheFileName);

	bool oldExists = fexists(oldfoldercache.c_str());
	bool newExists = fexists(newfoldercache.c_str());

	if (oldExists && !newExists)
	{
		// rename the old to the new
		Logger::Get().printf(Logger::Level::Error, "Need to rename old cache file: \"%s\"\n", oldfoldercache);
		MoveFileA(oldfoldercache.c_str(), newfoldercache.c_str());
	}
	else if (oldExists && newExists)
	{
		// delete the old
		Logger::Get().printf(Logger::Level::Error, "Need to delete old cache file: \"%s\"\n", oldfoldercache);
		DeleteFileA(oldfoldercache.c_str());
	}

	return newfoldercache;
}



//=================================================================================================
//=================================================================================================
#ifdef NO_LONGER_NEED_TO_RENAME_OLD_CACHE_FILES
inline std::string GetCacheFileName(const std::string folderName)
{
	std::string foldercache{folderName};

	foldercache.append(R"(\)");
	foldercache.append(pszLocalCacheFileName);

	return foldercache;
}
#else // NO_LONGER_NEED_TO_RENAME_OLD_CACHE_FILES
inline std::string GetCacheFileName(const std::string folderName)
{
	return CleanupOldCacheFiles(folderName);
}
#endif // NO_LONGER_NEED_TO_RENAME_OLD_CACHE_FILES


//=================================================================================================
//=================================================================================================
template <typename _ProcessFileFunctor> void ProcessFilesInFolder(const char *szFolderName, int depth, _ProcessFileFunctor processFileFunc)
{
	std::string fileSearchSpec{GetFolderWildcard(szFolderName)};

	HANDLE				hFile;
	WIN32_FIND_DATAA	fd;

	const size_t directoryStartSize = 32;
	std::vector<WIN32_FIND_DATAA>	fds;
	fds.reserve(directoryStartSize);

	hFile = FindFirstFileExA(fileSearchSpec.c_str(), FindExInfoBasic, reinterpret_cast<void *>(&fd), FindExSearchNameMatch, nullptr, FIND_FIRST_EX_LARGE_FETCH);
	if (hFile != INVALID_HANDLE_VALUE)
	{
		do
		{
			// skip reparse points
			if (fd.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)
			{
				continue;
			}

			for (size_t i = 0; i < ARRAYSIZE(szFileNamesToIgnore); i++)
			{
				if (0 == _stricmp(fd.cFileName, szFileNamesToIgnore[i]))
				{
					goto continueloop;
				}
			}

			fds.push_back(fd);

		continueloop:;

		} while (FindNextFileA(hFile, &fd));

		FindClose(hFile);
	}
	else
	{
		auto err = GetLastError();
		Logger::Get().printf(Logger::Level::Error, "Error %d accessing path \"%s\"\n", err, fileSearchSpec.c_str());
	}

	// folders
	for (auto &fd : fds)
	{
		if (0 == (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
		{
			processFileFunc(szFolderName, &fd, depth);
		}
	}

	// files
	for (auto &fd : fds)
	{
		if (0 != (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
		{
			processFileFunc(szFolderName, &fd, depth);
		}
	}

	return;
}

//=================================================================================================
//=================================================================================================
void ProcessFolder(const char *szFolderName, FileOnDiskSet &files, int depth, bool clean)
{
	// rename .bin to .md5
	auto foldercache = GetCacheFileName(szFolderName);

	Md5Cache			cache;
	Md5Cache			newCache;
	std::unordered_map<Path,const Md5CacheItem *> umap;
	std::unordered_map<Path,const Md5CacheItem *> *pumap = nullptr;

	{
		if (cache.Load(foldercache.c_str()))
		{
			for (auto &item : cache.Items)
			{
				umap[cache.GetFileName(item)] = &item;
			}

			pumap = &umap;

			if (clean)
			{
				newCache.Items.reserve(cache.Items.size());
				newCache.Strings.reserve(cache.Strings.size());
			}
		}
	}

	ProcessFilesInFolder(szFolderName, depth, [&pumap,&files,&clean,&cache,&newCache](const char *szFolderName, WIN32_FIND_DATAA *pfd, int depth)
	{
		ProcessFile(szFolderName, pfd, files, depth, pumap, clean);

		if (clean && pumap)
		{
			//WIN32_FIND_DATAA	&fd = *pfd;

			if (0 == (pfd->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
			{
				if (pumap->find(pfd->cFileName) != pumap->end())
				{
					const Md5CacheItem *pitem = (*pumap)[pfd->cFileName];

					if (pitem->Size != GetWin32FindDataFileSize(*pfd))
					{
						// remove it
						__nop();
					}
					else if (FileTimeDifference(pitem->Time, pfd->ftLastWriteTime) != 0)
					{
						// remove it
						__nop();
					}
					else
					{
						// add it to the new cache!!
						Md5CacheItem newItem = *pitem;
						newItem.Name = static_cast<long>(newCache.Strings.size());
						const char *pszName = cache.GetFileName(*pitem);
						size_t nameSize = strlen(pszName);
						newCache.Strings.insert(newCache.Strings.end(), pszName, pszName + nameSize + 1);
						newCache.Items.push_back(newItem);
					}
				}
				else
				{
					// it's a file that's not in the Md5Cache
					__nop();
				}
			}
		}
	});


	if (clean)
	{
		if (0 == newCache.Items.size() && (0 != cache.Items.size()))
		{
			// we have an MD5CACHE.md5 file, but we don't have ANY files that still work with it, so delete the file altogether!
			Logger::Get().printf(Logger::Level::Info, "Deleting  cache file (from %d to %d items) \"%s\"\n", cache.Items.size(), newCache.Items.size(), foldercache.c_str());
			if (!DeleteFileA(foldercache.c_str()))
			{
				Logger::Get().printf(Logger::Level::Info, "     couldn't delete.\n");
			}
		}
		else if (newCache.Items.size() != cache.Items.size())
		{
			// we're removed SOME items, so we need to re-write it
			Logger::Get().printf(Logger::Level::Info, "Rewriting cache file (from %d to %d items) \"%s\"\n", cache.Items.size(), newCache.Items.size(), foldercache.c_str());
			newCache.Save(foldercache.c_str());
		}
	}

	return;
}

//=================================================================================================
//=================================================================================================
void ProcessFile(const char *szFolderName, WIN32_FIND_DATAA *pfd, FileOnDiskSet &files, int depth, std::unordered_map<Path,const Md5CacheItem *> *pumap, bool clean)
{
	std::string fullPath{szFolderName};
	fullPath.append("\\");
	fullPath.append(pfd->cFileName);

	if (pfd->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
	{
		return ProcessFolder(fullPath.c_str(), files, depth+1, clean);
	}
	else
	{
		// see if the file has any hard links


		// add the file...
		FileOnDisk file;

		files.AddPathToStrings(file, fullPath.c_str(), strlen(szFolderName) + 1);

		file.Hashed		= false;
		file.Size		= GetWin32FindDataFileSize(*pfd);
		file.Time		= pfd->ftLastWriteTime;
		file.SubPath	= file.Path + files.RootPathLength + 1;

		if (nullptr != pumap)
		{
			if (pumap->find(pfd->cFileName) != pumap->end())
			{
				const Md5CacheItem *pitem = (*pumap)[pfd->cFileName];
				if (pitem->Size == file.Size)
				{
					if (pitem->Time == file.Time)
					{
						file.Hashed = true;
						file.Hash = pitem->Hash;
					}
				}
			}
		}

		files.Items.push_back(file);
	}

	return;
}


//=================================================================================================
//=================================================================================================
void FileOnDiskSet::QueryFileSystem(const char *pszRootPath, bool clean)
{
	this->RootPathLength = strlen(pszRootPath);
	ProcessFolder(pszRootPath, *this, 0, clean);
}

//=================================================================================================
// Adds the files from the input fileset to the output fileset
//=================================================================================================
void FileOnDiskSet::MergeFrom(const FileOnDiskSet &input)
{
	// first, make a map of paths to make sure that we don't add anything that already exists...
	std::unordered_map<Path, size_t> umap;

	// reserve space for the strings
	this->Strings.reserve(this->Strings.size() + input.Strings.size());

	if (true)
	{
		TimeThis t("Build the hashtable");
		umap.reserve(this->Items.size());

		int index = 0;
		for (auto &file : this->Items)
		{
			umap[this->GetFilePath(file)] = index;
			index++;
		}
	}

	for (auto &file : input.Items)
	{
		const char *pszPath = input.GetFilePath(file);
		if (umap.find(pszPath) != umap.end())
		{
			// error---we're trying to add something to the output list that's already there!
			assert(0);
			*(volatile int *)0;
		}

		FileOnDisk newFile = file;
		this->AddPathToStrings(newFile, pszPath, file.Name - file.Path);
		this->Items.push_back(newFile);
	}
}





//=================================================================================================
// for each file in the input fileset, if a hashed version is in the hashed fileset, copy that
// hash over, assuming timestamps match up
//=================================================================================================
void FileOnDiskSet::ApplyHashFrom(const FileOnDiskSet &hashedFiles)
{
	//=============================================================================================
	// build the hashtable
	std::unordered_map<Path, size_t> umap;

	if (true)
	{
		umap.reserve(hashedFiles.Items.size());

		int index = 0;
		for (auto &file : hashedFiles.Items)
		{
			const char *pszPath = hashedFiles.GetFilePath(file);
			umap[pszPath] = index;
			index++;
		}
	}

	//=============================================================================================
	// now, go through the files and find if there's a hash
	if (true)
	{
		for (auto &file : this->Items)
		{
			auto place = umap.find(this->GetFilePath(file));
			if (place != umap.end())
			{
				size_t index = place->second;
				const FileOnDisk &hashed = hashedFiles.Items[index];

				const char *psz1 = hashedFiles.GetFilePath(hashed);
				const char *psz2 = this->GetFilePath(file);
				assert(0 == _stricmp(psz1, psz2));

				if (FileTimeDifference(hashed.Time, file.Time) < 10 * ftMilliseconds)
				{
					if (hashed.Hashed)
					{
						file.Hashed = hashed.Hashed;
						file.Hash = hashed.Hash;
					}
				}
			}
		}
	}
}

//==================================================================================================
//==================================================================================================
bool AreAllOfTheseHardLinksToOneAnother(const std::vector<const char *> &filesToCheckForHardLinks)
{
	if (filesToCheckForHardLinks.size() < 2)
	{
		return true;
	}

	AutoCloseHandle	hfile1 = CreateFileA(filesToCheckForHardLinks[0], 0, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
	BY_HANDLE_FILE_INFORMATION info1 = {0};
	GetFileInformationByHandle(hfile1, &info1);

	for (size_t i=1; i<filesToCheckForHardLinks.size(); ++i)
	{
		AutoCloseHandle	hfile2 = CreateFileA(filesToCheckForHardLinks[i], 0, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);

		BY_HANDLE_FILE_INFORMATION info2 = {0};
		GetFileInformationByHandle(hfile2, &info2);

		if ((info1.nFileIndexHigh == info2.nFileIndexHigh) && (info1.nFileIndexLow == info2.nFileIndexLow))
		{
			// it's a link... don't give up
		}
		else
		{
			return false;
		}
	}

	return true;
}

//==================================================================================================
// UpdateHashedFiles
//
// Sort the input list, and go through the list and make sure that all files that have the same
// size have a hash. If a hash needs to be calculated, calculate it, and add it to the hash cache
//==================================================================================================
void FileOnDiskSet::UpdateHashedFiles(bool forceAll, bool verbose)
{
	// sort on size
	{
		TimeThis t("Sort 2");
		std::sort(this->Items.begin(), this->Items.end(), [&](FileOnDisk const &left, FileOnDisk const &right)
		{
			if (left.Size == right.Size)
			{
				return (_stricmp(this->GetFilePath(left), this->GetFilePath(right)) < 0);
			}

			return left.Size > right.Size;
		});
	}

	{
		TimeThis t("To calc hashes");
		int hashedCount = 0;
		long long byteCount = 0;

		for (size_t i = 0; i<this->Items.size(); i++)
		{
			FileOnDisk &file = this->Items[i];
			auto path = this->GetFilePath(i);
			auto name = this->GetFileName(i);
			auto subp = this->GetSubPathName(i);

			if (file.Hashed)
			{
				// no need... already got it
				continue;
			}

			if (0 == file.Size)
			{
				// no need... 0 sized file
				continue;
			}

			bool hashNeeded = forceAll;
			bool multipleFilesThatAreTheSameSize = false;

			// is this item's size the same as the last one's size?
			if ((i>0) && (file.Size == this->Items[i - 1].Size))
			{
				multipleFilesThatAreTheSameSize = true;
				hashNeeded = true;
			}
			// is this item's size the same as the next one's size?
			else if ((i + 1 < this->Items.size()) && (file.Size == this->Items[i + 1].Size))
			{
				multipleFilesThatAreTheSameSize = true;
				hashNeeded = true;
			}

			if (multipleFilesThatAreTheSameSize && !forceAll)
			{
				// go through and see if all the files in our list that are the same size are ACTUALLY hard links to
				// the same file
				size_t iStartIndex = i;
				while ((iStartIndex > 0) && (file.Size == this->Items[iStartIndex-1].Size))
				{
					--iStartIndex;
				}

				size_t iEndIndex = i;
				while ((iEndIndex < this->Items.size()) && (file.Size == this->Items[iEndIndex].Size))
				{
					++iEndIndex;
				}

				std::vector<const char *> filesToCheckForHardLinks;
				filesToCheckForHardLinks.reserve(1+iEndIndex-iStartIndex);

				for (size_t i2=iStartIndex; i2<iEndIndex; ++i2)
				{
					filesToCheckForHardLinks.push_back(this->GetFilePath(i2));
				}

				if (AreAllOfTheseHardLinksToOneAnother(filesToCheckForHardLinks))
				{
					verboseprintf("Skipping hash for \"%s\" because everything that's the same size is a hard link\n", this->GetFilePath(file));
					hashNeeded = false;
				}
			}

			if (hashNeeded)
			{
				TimeThis t("    Calulating hash");
				verboseprintf("Calculating hash for \"%s\"...\n", this->GetFilePath(file));
				file.Hashed = GetFileMd5Hash(this->GetFilePath(file), file.Hash, verbose);
				Logger::Get().printf(Logger::Level::Info, "(%13s) Calculating MD5 hash for \"%s\"\n", comma(file.Size), this->GetFilePath(file));
				hashedCount++;
				byteCount += file.Size;

				// now, save it to the local hash cache
				if (true)
				{
					Md5Cache cache;
					char szPath[maxString];
					const char *pszPath = this->GetFilePath(file);
					strncpy_s(szPath, pszPath, file.Name - file.Path);
					szPath[file.Name - file.Path] = 0;
					//strcat_s(szPath, "\\");
					strcat_s(szPath, pszLocalCacheFileName);

					if (cache.Load(szPath))
					{
						// we read something, so add ourselves to it
						std::unordered_map<Path, size_t> umap;

						size_t index = 0;
						for (auto &item : cache.Items)
						{
							umap[cache.GetFileName(item)] = index;
							index++;
						}

						if (umap.find(this->GetFileName(file)) == umap.end())
						{
							Md5CacheItem item = file;
							const char *pszName = this->GetFileName(file);
							item.Name = static_cast<long>(cache.Strings.size());
							cache.Strings.insert(cache.Strings.end(), pszName, pszName + strlen(pszName) + 1);
							cache.Items.push_back(item);
							cache.Save(szPath);
						}
						else
						{
							// it already exists... overwrite it
							index = umap[this->GetFileName(file)];
							Md5CacheItem item = file;
							item.Name = cache.Items[index].Name;
							cache.Items[index] = item;
							cache.Save(szPath);
						}
					}
					else
					{
						cache.Items.clear();
						cache.Strings.clear();

						Md5CacheItem item = file;
						item.Name = static_cast<long>(cache.Strings.size());
						const char *pszName = this->GetFileName(file);
						cache.Strings.insert(cache.Strings.end(), pszName, pszName + strlen(pszName) + 1);
						cache.Items.push_back(item);
						cache.Save(szPath);
					}
				}
			}
		}

		Logger::Get().printf(Logger::Level::Debug, "Calculated the hash of %d files for %s bytes.\n", hashedCount, comma(byteCount));
	}
}

//==================================================================================================
//==================================================================================================
void FileOnDiskSet::RemoveSetFromSet(const FileOnDiskSet &infiles)
{
	// create a path map of the infiles
	std::unordered_map<Path, size_t> umap;

	if (true)
	{
		TimeThis t("Build the hashtable");
		umap.reserve(infiles.Items.size());

		int index = 0;
		for (auto &file : infiles.Items)
		{
			const char *pszPath = infiles.GetFilePath(file);
			umap[pszPath] = index;
			index++;
		}
	}

	FileOnDiskSet output;
	output.Items.reserve(this->Items.size());
	output.Strings.reserve(this->Strings.size());

	for (auto &file : this->Items)
	{
		const char *pszPath = this->GetFilePath(file);

		if (umap.find(pszPath) == umap.end())
		{
			FileOnDisk newFile = file;
			output.AddPathToStrings(newFile, pszPath, file.Name - file.Path);
			output.Items.push_back(newFile);
		}
		else
		{
			#ifdef _DEBUG
			#endif
		}
	}

	this->Items.clear();
	this->Strings.clear();

	this->Items = std::move(output.Items);
	this->Strings = std::move(output.Strings);
}

//=================================================================================================
//=================================================================================================
bool FileOnDiskSet::UpdateFile(const FileOnDisk &file)
{
	Md5Cache cache;

	// this will represent the location of the Md5Cache file
	char szPath[maxString];

	// get the full path
	auto pszPath = this->GetFilePath(file);

	// copy the folder part
	strncpy_s(szPath, pszPath, file.Name - file.Path);

	// null-terminate it
	szPath[file.Name - file.Path] = 0;

	// add the name of the cache file
	strcat_s(szPath, pszLocalCacheFileName);

	// load the cache
	if (cache.Load(szPath))
	{
		auto filename = this->GetFileName(file);
		// find the item
		for (auto &i : cache.Items)
		{
			auto cachedName = cache.GetFileName(i);
			if (0 == _stricmp(cachedName, filename))
			{
				i = file;
				cache.Save(szPath);
				return true;
			}
		}

		// the filename wasn't found within the MD5 cache at all
		Md5CacheItem item = file;
		item.Name = static_cast<long>(cache.Strings.size());
		cache.Strings.insert(cache.Strings.end(), filename, filename + strlen(filename) + 1);
		cache.Items.push_back(item);
		cache.Save(szPath);
		return true;

	}
	else
	{
		// no MD5 cache file exists... let's create one!
		cache.Items.clear();
		cache.Strings.clear();

		Md5CacheItem item = file;
		item.Name = static_cast<long>(cache.Strings.size());
		const char *pszName = this->GetFileName(file);
		cache.Strings.insert(cache.Strings.end(), pszName, pszName + strlen(pszName) + 1);
		cache.Items.push_back(item);
		cache.Save(szPath);
		return true;
	}

	return false;
}



//==================================================================================================
//==================================================================================================
bool Md5Cache::Load(const char *pszFileName)
{
	bool result = false;

	HANDLE hFile = CreateFileA(pszFileName, GENERIC_READ, 0, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM, nullptr);

	if (nullptr != hFile && INVALID_HANDLE_VALUE != hFile)
	{
		HashCacheHeader header;
		DWORD dwBytes;

		// get the size
		LARGE_INTEGER filesize;
		GetFileSizeEx(hFile, &filesize);

		ReadFile(hFile, &header, sizeof(header), &dwBytes, nullptr);

		if (header.version == FILEONDISK_VERSION)
		{
			this->Items.clear();
			this->Strings.clear();

			size_t itemcount = static_cast<size_t>(header.numFiles);
			size_t stringSize = static_cast<size_t>(filesize.QuadPart) - (itemcount * sizeof(Md5CacheItem)) - sizeof(header);

			this->Items.reserve(itemcount);
			this->Strings.reserve(stringSize);

			size_t i = 0;

			Md5CacheItem item;

			for (i=0 ; i<itemcount ; i++)
			{
				ReadFile(hFile, &item, sizeof(item), &dwBytes, nullptr);
				assert(dwBytes == sizeof(item));
				assert(item.Size >= 0);
				if (item.Name < 0)
				{
					this->Items.clear();
					this->Strings.clear();
					CloseHandle(hFile);
					return false;
				}
				assert(item.Name >= 0);
				assert(item.Name < stringSize);
				this->Items.push_back(item);
			}

			// now, read in the strings...
			if (stringSize > 0)
			{
				this->Strings.assign(stringSize, 0);
				ReadFile(hFile, &this->Strings[0], static_cast<DWORD>(stringSize), &dwBytes, nullptr);
				assert(stringSize == dwBytes);
			}

			LARGE_INTEGER filePos = {0};
			SetFilePointerEx(hFile, filePos, &filePos, FILE_CURRENT);
			assert(filePos.QuadPart == filesize.QuadPart);

			result = true;
		}

		CloseHandle(hFile);
	}

	return result;
}


//==================================================================================================
//==================================================================================================
bool Md5Cache::Save(const char *pszFileName)
{
	bool result = false;

	HANDLE hFile = CreateFileA(pszFileName, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM, nullptr);

	if (nullptr != hFile && INVALID_HANDLE_VALUE != hFile)
	{
		DWORD dwBytes;

		HashCacheHeader header;
		header.numFiles = this->Items.size();
		header.version = FILEONDISK_VERSION;

		WriteFile(hFile, &header, sizeof(header), &dwBytes, nullptr);
		WriteFile(hFile, &this->Items[0], static_cast<DWORD>(sizeof(this->Items[0]) * this->Items.size()), &dwBytes, nullptr);
		WriteFile(hFile, &this->Strings[0], static_cast<DWORD>(sizeof(this->Strings[0]) * this->Strings.size()), &dwBytes, nullptr);

		CloseHandle(hFile);
	}

	return result;
}







//=================================================================================================
//=================================================================================================
inline BOOL SafeCryptReleaseContext(HCRYPTPROV &hProv, DWORD dwFlags)
{
	if (0 != hProv)
	{
		if (CryptReleaseContext(hProv, dwFlags))
		{
			hProv = 0;
			return TRUE;
		}
	}

	return FALSE;
}


//=================================================================================================
//=================================================================================================
inline BOOL SafeCryptDestroyHash(HCRYPTHASH &hHash)
{
	if (0 != hHash)
	{
		if (CryptDestroyHash(hHash))
		{
			hHash = 0;
			return TRUE;
		}
	}

	return FALSE;
}


//==================================================================================================
// CalcFileMd5Hash
//
// Read in the contents of a file, calculating the MD5 cache of its contents, and return the
// results
//==================================================================================================
bool CalcFileMd5Hash(const char *szFileName, Md5Hash &chash, bool verbose)
{
#if defined(_M_X64)
	const int bufferSize = 1024*1024;
#else
	const int bufferSize = 1024;
#endif

	HANDLE hFile = nullptr;
	bool result = false;
	HCRYPTPROV hProv = 0;
	HCRYPTHASH hHash = 0;
	std::vector<BYTE> buffer(bufferSize);
	DWORD cbRead = 0;
	const int hashLen = 16;
	unsigned char hash[hashLen];
	DWORD dwSize;

	// open the file
	hFile = CreateFileA(szFileName, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, nullptr);

	if (INVALID_HANDLE_VALUE == hFile)
	{
		goto Cleanup;
	}

	// Get handle to the crypto provider
	if (!CryptAcquireContext(&hProv, nullptr, nullptr, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT))
	{
		goto Cleanup;
	}

	// create the MD5 hash item
	if (!CryptCreateHash(hProv, CALG_MD5, 0, 0, &hHash))
	{
		goto Cleanup;
	}

	// loop, reading the file's contents
	while (ReadFile(hFile, &buffer[0], static_cast<DWORD>(buffer.size()), &cbRead, nullptr))
	{
		if (0 == cbRead)
		{
			break;
		}

		if (!CryptHashData(hHash, &buffer[0], cbRead, 0))
		{
			goto Cleanup;
		}
	}

	// get the final hash
	dwSize = hashLen;
	if (!CryptGetHashParam(hHash, HP_HASHVAL, hash, &dwSize, 0))
	{
		goto Cleanup;
	}

	if (dwSize != hashLen)
	{
		goto Cleanup;
	}

	// copy the result to the output buffer
	memcpy(chash._data, hash, sizeof(hash));
	result = true;

Cleanup:
	SafeCryptDestroyHash(hHash);
	SafeCryptReleaseContext(hProv, 0);
	SafeCloseHandle(hFile);

	return result;
}


//==================================================================================================
// GetFileMd5Hash
//
// Read in the contents of a file, calculating the MD5 cache of its contents, and return the
// results
//==================================================================================================
bool GetFileMd5Hash(const char *szFileName, Md5Hash &hash, bool verbose)
{
	// first, see if there are hard links...
	std::vector<std::string> hardlinks = GetAllHardLinksA(szFileName);
	for (auto &i : hardlinks)
	{
		if (GetCachedHash(i.c_str(), hash, verbose))
		{
			verboseprintf("No hash needed for \"%s\" because we got it from \"%s\"...\n", szFileName, i.c_str());
			return true;
		}
	}

	return CalcFileMd5Hash(szFileName, hash, verbose);
}




//==================================================================================================
// GetCachedHash
//
// See if a file already has a valid hash
//==================================================================================================
bool GetCachedHash(const char *szFileName, Md5Hash &hash, bool verbose)
{
	bool result = false;
	Md5Cache cache;
	std::unordered_map<Path,const Md5CacheItem *> umap;
	char szCacheFileName[maxString];

	strncpy_s(szCacheFileName, szFileName, ARRAYSIZE(szCacheFileName));

	// remove the filename
	char *p = szCacheFileName;
	while (*p) ++p;
	while (p>szCacheFileName && (*p!='\\')) p--;
	*p = 0;

	size_t fileNameOffset = p - szCacheFileName + 1;

	strcat_s(szCacheFileName, "\\");
	strcat_s(szCacheFileName, pszLocalCacheFileName);

	if (cache.Load(szCacheFileName))
	{
		for (auto &item : cache.Items)
		{
			umap[cache.GetFileName(item)] = &item;
		}

		// see if we have an entry for this item
		const char *pszFileNameOnly = &szFileName[fileNameOffset];
		auto cacheitem = umap.find(pszFileNameOnly);
		if (cacheitem != umap.end())
		{
			const Md5CacheItem *pcacheitem = umap[pszFileNameOnly];

			// now, see if it's valid
			HANDLE hFile = CreateFileA(szFileName, FILE_READ_ATTRIBUTES| FILE_READ_EA, 0, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM, nullptr);

			if (nullptr != hFile && INVALID_HANDLE_VALUE != hFile)
			{
				BY_HANDLE_FILE_INFORMATION fileInfo;
				if (GetFileInformationByHandle(hFile, &fileInfo))
				{
					LARGE_INTEGER filesize;
					filesize.HighPart = fileInfo.nFileSizeHigh;
					filesize.LowPart = fileInfo.nFileSizeLow;

					if ((filesize.QuadPart == pcacheitem->Size) && (fileInfo.ftLastWriteTime.dwHighDateTime == pcacheitem->Time.dwHighDateTime) && (fileInfo.ftLastWriteTime.dwLowDateTime == pcacheitem->Time.dwLowDateTime))
					{
						size_t hashsize = sizeof(pcacheitem->Hash);
						hash = pcacheitem->Hash;
						result = true;
					}
					else
					{
						__nop();
					}
				}
				CloseHandle(hFile);
			}
		}
	}

	return result;
}


//=================================================================================================
//=================================================================================================
static void CleanFolderRecurse(const char *szFolderName)
{
	Md5Cache cache;
	Md5Cache newCache;
	std::unordered_map<Path,const Md5CacheItem *> umap;
	std::unordered_map<Path,const Md5CacheItem *> *pumap = nullptr;
	auto foldercache = GetCacheFileName(szFolderName);

	{
		if (cache.Load(foldercache.c_str()))
		{
			for (auto &item : cache.Items)
			{
				umap[cache.GetFileName(item)] = &item;
			}

			pumap = &umap;

			newCache.Items.reserve(cache.Items.size());
			newCache.Strings.reserve(cache.Strings.size());
		}
	}

	ProcessFilesInFolder(szFolderName, 0, [&umap,&cache,&newCache](const char *szFolderName, WIN32_FIND_DATAA *pfd, int depth)
	{
		WIN32_FIND_DATAA	&fd = *pfd;

		if (0 != (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
		{
			size_t newFolderStringSize = strlen(szFolderName) + strlen(fd.cFileName) + 2;
			std::vector<char> szNewPathName(newFolderStringSize);
			char *pszNewPathName = &szNewPathName[0];
			strcpy_s(pszNewPathName, newFolderStringSize, szFolderName);
			strcat_s(pszNewPathName, newFolderStringSize, "\\");
			strcat_s(pszNewPathName, newFolderStringSize, fd.cFileName);

			CleanFolderRecurse(pszNewPathName);
		}
		else
		{
			if (umap.find(fd.cFileName) != umap.end())
			{
				const Md5CacheItem *pitem = umap[fd.cFileName];

				if (pitem->Size != GetWin32FindDataFileSize(fd))
				{
					// remove it
					__nop();
				}
				else if (FileTimeDifference(pitem->Time, fd.ftLastWriteTime) != 0)
				{
					// remove it
					__nop();
				}
				else
				{
					// add it to the new cache!!

					Md5CacheItem newItem = *pitem;
					newItem.Name = static_cast<long>(newCache.Strings.size());
					const char *pszName = cache.GetFileName(*pitem);
					size_t nameSize = strlen(pszName);
					newCache.Strings.insert(newCache.Strings.end(), pszName, pszName + nameSize + 1);
					newCache.Items.push_back(newItem);
				}
			}
			else
			{
				// it's a file that's not in the Md5Cache
				__nop();
			}
		}
	});

	if (0 == newCache.Items.size() && (0 != cache.Items.size()))
	{
		// we have an MD5CACHE.md5 file, but we don't have ANY files that still work with it, so delete the file altogether!
		Logger::Get().printf(Logger::Level::Info, "Deleting  cache file (from %d to %d items) \"%s\"\n", cache.Items.size(), newCache.Items.size(), foldercache.c_str());
		if (!DeleteFileA(foldercache.c_str()))
		{
			Logger::Get().printf(Logger::Level::Info, "     couldn't delete.\n");
		}
	}
	else if (newCache.Items.size() != cache.Items.size())
	{
		// we're removed SOME items, so we need to re-write it
		Logger::Get().printf(Logger::Level::Info, "Rewriting cache file (from %d to %d items) \"%s\"\n", cache.Items.size(), newCache.Items.size(), foldercache.c_str());
		newCache.Save(foldercache.c_str());
	}

	return;
}


//=================================================================================================
//=================================================================================================
void CleanCacheFiles(const char *pszRootPath)
{
	CleanFolderRecurse(pszRootPath);
}
