#define FILEONDISK_VERSION		0x00000100

//=================================================================================================
// Given a C string (case-insensitive char *) representing a path, get the hash value of it (the
// hash value must be the same for any two paths that are equal even if there are case differences)
//=================================================================================================
inline size_t PathHash(const char *pszPath)
{
	const size_t maxpath = 2048;

	assert(strlen(pszPath) < maxpath);

	char lowercase[maxpath];
	char *p = lowercase;

	while (*pszPath && ((p-lowercase)<maxpath))
	{
		*p++ = tolower(*pszPath++);
	}

	return std::_Hash_seq(reinterpret_cast<unsigned char *>(lowercase), p-lowercase);
}

//=================================================================================================
// Functions for allowing the std::unordered map to use C-style strings as their keys, and in
// a case-insensitive way
//=================================================================================================
namespace std
{
	template <> struct hash<const char *> : public unary_function<const char *, size_t>
	{
		size_t operator()(const char * &value) const
		{
			return PathHash(value);
		}
	};

	template <> struct equal_to<const char *> : public unary_function<const char *, bool>
	{
		bool operator()(const char *&x, const char *&y) const
		{
			return (0 == _stricmp(x, y));
		}
	};

	template <> struct hash<char *> : public unary_function<char *, size_t>
	{
		size_t operator()(char * value) const
		{
			return PathHash(value);
		}
	};

	template <> struct equal_to<char *> : public unary_function<char *, bool>
	{
		bool operator()(char *x, char *y) const
		{
			return (0 == _stricmp(x, y));
		}
	};

} // namespace std



//=================================================================================================
//=================================================================================================
struct HashCacheHeader
{
	long long		version;
	long long		numFiles;
};

//=================================================================================================
// File on Disk structure
//
// Represents a file in the filesystem.
//
// I considered using a "char *" instead of an offset (size_t) for the Name and Path. It certainly
// would make the code look cleaner. However, since all the names are in a dynamic array (i.e.,
// a std::vector), the base location of the vector *CAN* change when items are added, and thus
// all those pointers would become invalid.
//
//=================================================================================================
struct FileOnDisk
{
	bool				Hashed;
	long long			Size;
	FILETIME			Time;
	unsigned char		Hash[16];
	size_t				Name;
	size_t				Path;

	// not in the MD5cache...
	mutable DWORD   			nNumberOfLinks;
	mutable unsigned long long	nFileIndex;
};

//=================================================================================================
//=================================================================================================
struct FileOnDiskSet
{
	std::vector<FileOnDisk>		Items;
	std::vector<char>			Strings;

	inline char *GetFilePath(size_t index)
	{
		assert(index < this->Items.size());
		assert(this->Items[index].Path < this->Strings.size());

		return &this->Strings[this->Items[index].Path];
	}

	inline char *GetFilePath(const FileOnDisk &file)
	{
		//assert(&file >= &this->Items[0]);
		//assert(&file <= &this->Items[this->Items.size()-1]);
		assert(file.Path < this->Strings.size());

		return &this->Strings[file.Path];
	}

	inline char *GetFileName(size_t index)
	{
		assert(index < this->Items.size());
		assert(this->Items[index].Name < this->Strings.size());

		return &this->Strings[this->Items[index].Name];
	}

	inline char *GetFileName(const FileOnDisk &file)
	{
		assert(file.Name < this->Strings.size());
		return &this->Strings[file.Name];
	}

	inline const char *GetFilePath(size_t index) const
	{
		assert(index < this->Items.size());
		assert(this->Items[index].Path < this->Strings.size());
		return &this->Strings[this->Items[index].Path];
	}

	inline const char *GetFilePath(const FileOnDisk &file) const
	{
		assert(file.Path < this->Strings.size());
		return &this->Strings[file.Path];
	}

	inline const char *GetFileName(size_t index) const
	{
		assert(index < this->Items.size());
		assert(this->Items[index].Name < this->Strings.size());
		return &this->Strings[this->Items[index].Name];
	}

	inline const char *GetFileName(const FileOnDisk &file) const
	{
		assert(file.Name < this->Strings.size());
		return &this->Strings[file.Name];
	}


	void AddPathToStrings(FileOnDisk &file, const char *szPath, size_t nameOffset)
	{
		size_t offset	= this->Strings.size();
		file.Path		= offset;
		file.Name		= file.Path + nameOffset;
		this->Strings.insert(this->Strings.end(), szPath, szPath+strlen(szPath)+1);
	}


public:
	// add the files from the input list to ours
	void MergeFrom(const FileOnDiskSet &input);

	// for any files in both our list and the hashed list, apply the hash list's hashes to ours (if
	// applicable)
	void ApplyHashFrom(const FileOnDiskSet &hashedFiles);

	// remove all files from our set that are in the infiles set
	void RemoveSetFromSet(const FileOnDiskSet &infiles);

	// calculate the hash for all files in the set that need it
	void UpdateHashedFiles(bool forceAll=false, bool verbose=false);

	// read in from the file system (including relevant md5cache.bin files)
	void QueryFileSystem(const char *pszRootPath);

	//==============================================================================================
	//==============================================================================================
	inline void CheckStrings(void)
	{
#ifdef _DEBUG
		for_each(this->Items.begin(), this->Items.end(), [&](const FileOnDisk &file)
		{
			if (file.Path > this->Strings.size())
			{
				assert(0);
			}
		});
#endif
	}
};

//=================================================================================================
//=================================================================================================
struct MD5CacheItem
{
	long long			Size;
	FILETIME			Time;
	unsigned char		Hash[16];
	unsigned long		Name;
	unsigned long		Filler;

	MD5CacheItem(const FileOnDisk &file)
	{
		this->Size = file.Size;
		this->Time = file.Time;
		memcpy(this->Hash, file.Hash, sizeof(this->Hash));
	}

	MD5CacheItem(){}
};

//=================================================================================================
//=================================================================================================
struct MD5Cache
{
	std::vector<MD5CacheItem>	Items;
	std::vector<char>			Strings;

	bool Load(const char *pszFileName);
	bool Read(const char *pszFileName) { return this->Load(pszFileName);}
	bool Save(const char *pszFileName);

	inline char *GetFileName(size_t index)
	{
		assert(index < this->Items.size());
		assert(this->Items[index].Name < this->Strings.size());

		return &this->Strings[this->Items[index].Name];
	}

	inline char *GetFileName(const MD5CacheItem &file)
	{
		//assert(&file >= &this->Items[0]);
		//assert(&file <= &this->Items[this->Items.size()-1]);
		assert(file.Name < this->Strings.size());

		return &this->Strings[file.Name];
	}
};



//==================================================================================================
//==================================================================================================
__declspec(selectany) const char * pszLocalCacheFileName = "md5cache.bin";

extern void CleanCacheFiles(const char *pszRootPath);
