// FileOnDisk.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"

#include <utilities.h>
#include <FileOnDisk.h>

const size_t maxString = 1024;
static const char *szFileNamesToIgnore[] = { ".","..","desktop.ini","md5cache.bin","folder.bin","folder.jpg",pszLocalCacheFileName };

//=================================================================================================
//=================================================================================================
static void ProcessFolder(const char *szName, FileOnDiskSet &files, int depth);
static void ProcessFile(const char *szFolderName, WIN32_FIND_DATAA *pfd, FileOnDiskSet &files, int depth, std::unordered_map<char *,const MD5CacheItem *> *pumap);
static bool CalcFileMd5Hash(const char *szFileName, unsigned char *pHash);

#define MAX_STRING 1024

#define MAKELONGLONG(lo,hi) ((long long)(((unsigned long long)(lo)) | (((unsigned long long)(hi)) << 32)))

//=================================================================================================
//=================================================================================================
void ProcessFolder(const char *szFolderName, FileOnDiskSet &files, int depth)
{
	char				szFileSearchSpec[maxString];
	char				foldercache[maxString];
	HANDLE				hFile;
	MD5Cache			cache;
	WIN32_FIND_DATAA	fd;
	std::unordered_map<char *,const MD5CacheItem *> umap;
	std::unordered_map<char *,const MD5CacheItem *> *pumap = nullptr;

	{
		strcpy_s(foldercache, szFolderName);
		strcat_s(foldercache, "\\");
		strcat_s(foldercache, pszLocalCacheFileName);
		cache.Read(foldercache);

		for_each(cache.Items.begin(), cache.Items.end(), [&](MD5CacheItem &item)
		{
			umap[cache.GetFileName(item)] = &item;
		});

		pumap = &umap;
	}

	strcpy_s(szFileSearchSpec, szFolderName);
	strcat_s(szFileSearchSpec, "\\*.*");

	const size_t directoryStartSize = 32;
	std::vector<WIN32_FIND_DATAA>	fds;
	fds.reserve(directoryStartSize);

	hFile = FindFirstFileExA(szFileSearchSpec, FindExInfoBasic, reinterpret_cast<void *>(&fd), FindExSearchNameMatch, nullptr, FIND_FIRST_EX_LARGE_FETCH);
	if (hFile != INVALID_HANDLE_VALUE)
	{
		do
		{
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

	for_each(fds.begin(), fds.end(), [&](WIN32_FIND_DATAA &fd)
	{
		if (0 == (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
		{
			ProcessFile(szFolderName, &fd, files, depth, pumap);
		}
	});

	for_each(fds.begin(), fds.end(), [&](WIN32_FIND_DATAA &fd)
	{
		if (0 != (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
		{
			ProcessFile(szFolderName, &fd, files, depth, pumap);
		}
	});

	return;
}

//=================================================================================================
//=================================================================================================
void ProcessFile(const char *szFolderName, WIN32_FIND_DATAA *pfd, FileOnDiskSet &files, int depth, std::unordered_map<char *,const MD5CacheItem *> *pumap)
{
	char szFullPath[maxString];

	strcpy_s(szFullPath, szFolderName);
	strcat_s(szFullPath, "\\");
	strcat_s(szFullPath, pfd->cFileName);

	if (pfd->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
	{
		return ProcessFolder(szFullPath, files, depth+1);
	}
	else
	{
		// add the file...
		FileOnDisk file;

		files.AddPathToStrings(file, szFullPath, strlen(szFolderName) + 1);

		file.Hashed		= false;
		file.Size		= MAKELONGLONG(pfd->nFileSizeLow, pfd->nFileSizeHigh);
		file.Time		= pfd->ftLastWriteTime;

		if (nullptr != pumap)
		{
			if (pumap->find(pfd->cFileName) != pumap->end())
			{
				const MD5CacheItem *pitem = (*pumap)[pfd->cFileName];
				if (pitem->Size == file.Size)
				{
					if (pitem->Time == file.Time)
					{
						file.Hashed = true;
						memcpy(file.Hash, pitem->Hash, sizeof(file.Hash));
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
void FileOnDiskSet::QueryFileSystem(const char *pszRootPath)
{
	ProcessFolder(pszRootPath, *this, 0);
}

//=================================================================================================
// Adds the files from the input fileset to the output fileset
//=================================================================================================
void FileOnDiskSet::MergeFrom(const FileOnDiskSet &input)
{
	// first, make a map of paths to make sure that we don't add anything that already exists...
	std::unordered_map<char *, size_t> umap;

	// reserve space for the strings
	this->Strings.reserve(this->Strings.size() + input.Strings.size());

	if (true)
	{
		TimeThis t("Build the hashtable");
		umap.reserve(this->Items.size());

		int index = 0;
		for_each(this->Items.begin(), this->Items.end(), [&](const FileOnDisk &file)
		{
			umap[this->GetFilePath(file)] = index;
			index++;
		});
	}

	for_each(input.Items.begin(), input.Items.end(), [&](const FileOnDisk &file)
	{
		char *pszPath = const_cast<char *>(input.GetFilePath(file));
		if (umap.find(pszPath) != umap.end())
		{
			// error---we're trying to add something to the output list that's already there!
			assert(0);
			*(volatile int *)0;
		}

		FileOnDisk newFile = file;
		this->AddPathToStrings(newFile, pszPath, file.Name - file.Path);
		this->Items.push_back(newFile);
	});

}





//=================================================================================================
// for each file in the input fileset, if a hashed version is in the hashed fileset, copy that
// hash over, assuming timestamps match up
//=================================================================================================
void FileOnDiskSet::ApplyHashFrom(const FileOnDiskSet &hashedFiles)
{
	//=============================================================================================
	//=============================================================================================
	// build the hashtable
	std::unordered_map<char *, size_t> umap;

	if (true)
	{
		umap.reserve(hashedFiles.Items.size());

		int index = 0;
		for_each(hashedFiles.Items.begin(), hashedFiles.Items.end(), [&](const FileOnDisk &file)
		{
			char *pszPath = const_cast<char *>(hashedFiles.GetFilePath(file));
			umap[pszPath] = index;
			index++;
		});
	}

	// now, go through the files and find if there's a hash
	if (true)
	{
		for_each(this->Items.begin(), this->Items.end(), [&](FileOnDisk &file)
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
						memcpy(file.Hash, hashed.Hash, sizeof(file.Hash));
					}
				}
			}
		});
	}
}

//==================================================================================================
// UpdateHashedFiles
//
// Sort the input list, and go through the list and make sure that all files that have the same
// size have a hash. If a hash needs to be calculated, calculate it, and add it to the hash cache
//==================================================================================================
void FileOnDiskSet::UpdateHashedFiles(void)
{
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

			bool hashNeeded = false;

			if ((i>0) && (file.Size == this->Items[i - 1].Size))
			{
				hashNeeded = true;
			}
			else if ((i + 1 < this->Items.size()) && (file.Size == this->Items[i + 1].Size))
			{
				hashNeeded = true;
			}

			if (hashNeeded)
			{
				TimeThis t("    Calulating hash");
				file.Hashed = CalcFileMd5Hash(this->GetFilePath(file), file.Hash);
				Logger::Get().printf(Logger::Level::Info, "(%12s) Calculating MD5 hash for \"%s\"\n", comma(file.Size), this->GetFilePath(file));
				hashedCount++;
				byteCount += file.Size;

				// now, save it to the local hash cache
				if (true)
				{
					MD5Cache cache;
					char szPath[maxString];
					char *pszPath = this->GetFilePath(file);
					strncpy_s(szPath, pszPath, file.Name - file.Path);
					szPath[file.Name - file.Path] = 0;
					//strcat_s(szPath, "\\");
					strcat_s(szPath, pszLocalCacheFileName);

					if (cache.Load(szPath))
					{
						// we read something, so add ourselves to it
						std::unordered_map<char *, size_t> umap;

						size_t index = 0;
						for_each(cache.Items.begin(), cache.Items.end(), [&](MD5CacheItem &item)
						{
							umap[cache.GetFileName(item)] = index;
							index++;
						});

						if (umap.find(this->GetFileName(file)) == umap.end())
						{
							MD5CacheItem item = file;
							char *pszName = this->GetFileName(file);
							item.Name = static_cast<long>(cache.Strings.size());
							cache.Strings.insert(cache.Strings.end(), pszName, pszName + strlen(pszName) + 1);
							cache.Items.push_back(item);
							cache.Save(szPath);
						}
						else
						{
							// it already exists... overwrite it
							index = umap[this->GetFileName(file)];
							MD5CacheItem item = file;
							item.Name = cache.Items[index].Name;
							cache.Items[index] = item;
							cache.Save(szPath);
						}
					}
					else
					{
						cache.Items.clear();
						cache.Strings.clear();

						MD5CacheItem item = file;
						item.Name = static_cast<long>(cache.Strings.size());
						char *pszName = this->GetFileName(file);
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
	std::unordered_map<char *, size_t> umap;

	if (true)
	{
		TimeThis t("Build the hashtable");
		umap.reserve(infiles.Items.size());

		int index = 0;
		for_each(infiles.Items.begin(), infiles.Items.end(), [&](const FileOnDisk &file)
		{
			char *pszPath = const_cast<char *>(infiles.GetFilePath(file));
			umap[pszPath] = index;
			index++;
		});
	}

	FileOnDiskSet output;
	output.Items.reserve(this->Items.size());
	output.Strings.reserve(this->Strings.size());

	for_each(this->Items.begin(), this->Items.end(), [&](const FileOnDisk &file)
	{
		char *pszPath = const_cast<char *>(this->GetFilePath(file));

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
	});

	this->Items.clear();
	this->Strings.clear();

	this->Items = std::move(output.Items);
	this->Strings = std::move(output.Strings);
}

//==================================================================================================
//==================================================================================================
bool MD5Cache::Load(const char *pszFileName)
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
			size_t stringSize = static_cast<size_t>(filesize.QuadPart) - (itemcount * sizeof(MD5CacheItem)) - sizeof(header);

			this->Items.reserve(itemcount);
			this->Strings.reserve(stringSize);

			size_t i = 0;

			MD5CacheItem item;

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
bool MD5Cache::Save(const char *pszFileName)
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
bool CalcFileMd5Hash(const char *szFileName, unsigned char *pHash)
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
	memcpy(pHash, hash, sizeof(hash));
	result = true;

Cleanup:
	SafeCryptDestroyHash(hHash);
	SafeCryptReleaseContext(hProv, 0);
	SafeCloseHandle(hFile);

	return result;
}






































































































static void CleanFolder(const char *szName);

//=================================================================================================
//=================================================================================================
void CleanFolder(const char *szFolderName)
{
	MD5Cache cache;
	MD5Cache newCache;
	char szCacheFileName[maxString];
	std::unordered_map<char *,const MD5CacheItem *> umap;
	std::unordered_map<char *,const MD5CacheItem *> *pumap = nullptr;

	if (true)
	{
		strncpy_s(szCacheFileName, szFolderName, ARRAYSIZE(szCacheFileName));
		strcat_s(szCacheFileName, "\\");
		strcat_s(szCacheFileName, pszLocalCacheFileName);

		if (cache.Load(szCacheFileName))
		{
			for_each(cache.Items.begin(), cache.Items.end(), [&](MD5CacheItem &item)
			{
				umap[cache.GetFileName(item)] = &item;
			});

			pumap = &umap;

			newCache.Items.reserve(cache.Items.size());
			newCache.Strings.reserve(cache.Strings.size());
		}
	}

	char				szFileSearchSpec[maxString];
	HANDLE				hFile;
	WIN32_FIND_DATAA	fd;

	strcpy_s(szFileSearchSpec, szFolderName);
	strcat_s(szFileSearchSpec, "\\*.*");

	hFile = FindFirstFileExA(szFileSearchSpec, FindExInfoBasic, reinterpret_cast<void *>(&fd), FindExSearchNameMatch, nullptr, FIND_FIRST_EX_LARGE_FETCH);
	if (hFile != INVALID_HANDLE_VALUE)
	{
		do
		{
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

			if (0 != (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
			{
				size_t newFolderStringSize = strlen(szFolderName) + strlen(fd.cFileName) + 2;
				std::vector<char> szNewPathName(newFolderStringSize);
				char *pszNewPathName = &szNewPathName[0];
				strcpy_s(pszNewPathName, newFolderStringSize, szFolderName);
				strcat_s(pszNewPathName, newFolderStringSize, "\\");
				strcat_s(pszNewPathName, newFolderStringSize, fd.cFileName);

				CleanFolder(pszNewPathName);
			}
			else
			{
				if (umap.find(fd.cFileName) != umap.end())
				{
					const MD5CacheItem *pitem = umap[fd.cFileName];

					if (pitem->Size != MAKELONGLONG(fd.nFileSizeLow, fd.nFileSizeHigh))
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

						MD5CacheItem newItem = *pitem;
						newItem.Name = static_cast<long>(newCache.Strings.size());
						const char *pszName = cache.GetFileName(*pitem);
						size_t nameSize = strlen(pszName);
						newCache.Strings.insert(newCache.Strings.end(), pszName, pszName + nameSize + 1);
						newCache.Items.push_back(newItem);
					}
				}
				else
				{
					// it's a file that's not in the MD5cache
					__nop();
				}
			}


		continueloop:;

		} while (FindNextFileA(hFile, &fd));

		FindClose(hFile);

		if (0 == newCache.Items.size() && (0 != cache.Items.size()))
		{
			// we have an MD5CACHE.bin file, but we don't have ANY files that still work with it, so delete the file altogether!
			Logger::Get().printf(Logger::Level::Info, "Deleting  cache file (from %d to %d items) \"%s\"\n", cache.Items.size(), newCache.Items.size(), szCacheFileName);
			if (!DeleteFileA(szCacheFileName))
			{
				Logger::Get().printf(Logger::Level::Info, "     couldn't delete.\n");
			}
		}
		else if (newCache.Items.size() != cache.Items.size())
		{
			// we're removed SOME items, so we need to re-write it
			Logger::Get().printf(Logger::Level::Info, "Rewriting cache file (from %d to %d items) \"%s\"\n", cache.Items.size(), newCache.Items.size(), szCacheFileName);
			newCache.Save(szCacheFileName);
		}
	}

	return;
}





//=================================================================================================
//=================================================================================================
void CleanCacheFiles(const char *pszRootPath)
{
	CleanFolder(pszRootPath);
}


