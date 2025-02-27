#define FILEONDISK_VERSION		0x00000100

//=====================================================================================================================================================================================================
// Path represents a path. It's just a case-insensitive string
//
// We use it instead of a "char *" because it abstracts what it is, and prevents the compiler
// from using any of a variation of "char *" operators to determine equality and compute hashes
// for hashtables
//
// Without it, we would need to implement:
//
// => 48 different specialized template "std::equal_to" operator() functions
// => 6 different specialized template "std::hash" operation() functions
//
//=====================================================================================================================================================================================================
class Path
{
private:
	//=================================================================================================================================================================================================
	// Internal data
	//=================================================================================================================================================================================================
	const char *_p;


public:
	//=================================================================================================================================================================================================
	// Constructors...
	//=================================================================================================================================================================================================
	Path() : _p(nullptr) {}
	Path(const char *p) : _p(p) {}
	Path(char *p) : _p(p) {}

	//=================================================================================================================================================================================================
	// Destructor
	//=================================================================================================================================================================================================
	~Path() = default;

	//=================================================================================================================================================================================================
	// Hash computation for use in an unordered_map
	//
	// The hash value must be the same for any two paths that are equal even if
	// there are case differences
	//=================================================================================================================================================================================================
	inline size_t GetHash() const
	{
		const size_t maxpath = 2048;

		assert(strlen(_p) < maxpath);

		char lowercase[maxpath];
		char *p = lowercase;
		const char *pszPath = _p;

		while (*pszPath && ((p-lowercase)<maxpath))
		{
			*p++ = tolower(*pszPath++);
		}

		//return std::_Hash_seq(reinterpret_cast<unsigned char *>(lowercase), p - lowercase);
		return std::_Hash_array_representation(reinterpret_cast<unsigned char *>(lowercase), p - lowercase);
	}

	//=================================================================================================================================================================================================
	// equality operators
	//=================================================================================================================================================================================================
	inline bool operator ==(const Path &other) const
	{
		return (0 == _stricmp(this->_p, other._p));
	}
};

//=====================================================================================================================================================================================================
// Md5Hash
//
// A class representing an MD5 hash, which is a 16-byte blob.
//=====================================================================================================================================================================================================
class Md5Hash
{
public:
	unsigned char	_data[16];
	static Md5Hash NullHash;

	//=================================================================================================================================================================================================
	// constructors
	//=================================================================================================================================================================================================
	Md5Hash()
	{
		memset(_data, 0, sizeof(_data));
	}

	Md5Hash(const Md5Hash &other)
	{
		memcpy(_data, other._data, sizeof(_data));
	}

	Md5Hash(Md5Hash &&other) noexcept
	{
		memcpy(_data, other._data, sizeof(_data));
	}


	//=================================================================================================================================================================================================
	// equality operators
	//=================================================================================================================================================================================================
	inline bool operator ==(const Md5Hash& other) const
	{
		return (0 == memcmp(_data, other._data, sizeof(_data)));
	}


	//=================================================================================================================================================================================================
	// assignment operators
	//=================================================================================================================================================================================================
	inline void operator =(const Md5Hash &other)
	{
		memcpy(_data, other._data, sizeof(_data));
	}

	inline void operator =(Md5Hash &&other)
	{
		memcpy(_data, other._data, sizeof(_data));
	}


	//=================================================================================================================================================================================================
	// Other functions
	//=================================================================================================================================================================================================
	inline char *ToString() const
	{
		const int numStrings = 8; // must be a power of two!!
		const int stringLength = 33;
		static char string[numStrings][stringLength];
		static int	index=-1;
		static const char hex[] = "0123456789ABCDEF";

		// make sure the numstrings is a power of two
		C_ASSERT(0 == (numStrings & (numStrings - 1)));

		index = (index+1)&(numStrings-1);

		char *p = string[index];

		for (int i=0 ; i<16 ; i++)
		{
			*p++ = hex[_data[i] >> 4];
			*p++ = hex[_data[i] & 0x0F];
		}

		*p++ = 0;

		return string[index];
	}
};

//=====================================================================================================================================================================================================
// Functions for allowing the std::unordered map to use C-style strings as their keys, and in
// a case-insensitive way
//=====================================================================================================================================================================================================
namespace std
{
	template <> struct hash<Path>
	{
		size_t operator()(Path &p) const { return p.GetHash(); }
		size_t operator()(const Path &p) const { return p.GetHash(); }
	};
} // namespace std



//=====================================================================================================================================================================================================
//=====================================================================================================================================================================================================
struct HashCacheHeader
{
	long long		version;
	long long		numFiles;
};

//=====================================================================================================================================================================================================
// File on Disk structure
//
// Represents a file in the filesystem.
//
// I considered using a "char *" instead of an offset (size_t) for the Name and Path. It certainly
// would make the code look cleaner. However, since all the names are in a dynamic array (i.e.,
// a std::vector), the base location of the vector *CAN* change when items are added, and thus
// all those pointers would become invalid.
//
//=====================================================================================================================================================================================================
struct FileOnDisk
{
	bool						Hashed;
	long long					Size;
	FILETIME					Time;
	Md5Hash						Hash;
	size_t						Name;
	size_t						Path;

	// not in the Md5Cache...
	size_t						SubPath;
	mutable DWORD   			nNumberOfLinks;
	mutable unsigned long long	nFileIndex;

	inline char *HashToString() const
	{
		if (this->Hashed)
		{
			return this->Hash.ToString();
		}
		else
		{
			return "                                ";
		}
	}
};

//=====================================================================================================================================================================================================
//=====================================================================================================================================================================================================
struct FileOnDiskSet
{
	std::vector<FileOnDisk>		Items;
	std::vector<char>			Strings;
	size_t						RootPathLength;

	//=================================================================================================================================================================================================
	// Const/non versions
	//=================================================================================================================================================================================================
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

	inline const char *GetSubPathName(size_t index) const
	{
		assert(index < this->Items.size());
		assert(this->Items[index].SubPath < this->Strings.size());
		return &this->Strings[this->Items[index].SubPath];
	}

	inline const char *GetSubPathName(const FileOnDisk &file) const
	{
		assert(file.SubPath < this->Strings.size());
		return &this->Strings[file.SubPath];
	}

	inline const std::string GetFolderName(size_t index) const
	{
		const char *p0 = this->GetFilePath(index);
		const char *p1 = this->GetFileName(index)-1;
		return std::string(p0, p1-p0);
	}

	inline const std::string GetFolderName(const FileOnDisk &file) const
	{
		const char *p0 = this->GetFilePath(file);
		const char *p1 = this->GetFileName(file)-1;
		return std::string(p0, p1-p0);
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

	// read in from the file system (including relevant md5cache.md5 files)
	void QueryFileSystem(const char *pszRootPath, bool clean=false);

	// update a single one
	bool UpdateFile(const FileOnDisk &file);

	//=================================================================================================================================================================================================
	//=================================================================================================================================================================================================
	inline void CheckStrings(void)
	{
#ifdef _DEBUG
		for (auto &file : this->Items)
		{
			if (file.Path > this->Strings.size())
			{
				assert(0);
			}
		}
#endif
	}
};

//=====================================================================================================================================================================================================
//=====================================================================================================================================================================================================
struct Md5CacheItem
{
	long long			Size;
	FILETIME			Time;
	Md5Hash				Hash;
	unsigned long		Name;
	unsigned long		Filler;

	Md5CacheItem(const FileOnDisk &file)
	{
		this->Size = file.Size;
		this->Time = file.Time;
		this->Hash = file.Hash;
	}

	Md5CacheItem(){}
};

//=====================================================================================================================================================================================================
//=====================================================================================================================================================================================================
struct Md5Cache
{
	std::vector<Md5CacheItem>	Items;
	std::vector<char>			Strings;

	bool Load(const char *pszFileName);
	bool Save(const char *pszFileName);

	inline char *GetFileName(size_t index)
	{
		assert(index < this->Items.size());
		assert(this->Items[index].Name < this->Strings.size());

		return &this->Strings[this->Items[index].Name];
	}

	inline char *GetFileName(const Md5CacheItem &file)
	{
		//assert(&file >= &this->Items[0]);
		//assert(&file <= &this->Items[this->Items.size()-1]);
		assert(file.Name < this->Strings.size());

		return &this->Strings[file.Name];
	}
};



//=====================================================================================================================================================================================================
//=====================================================================================================================================================================================================
__declspec(selectany) const char * pszLocalCacheFileName = "md5cache.md5";
__declspec(selectany) const char * pszOldLocalCacheFileName = "md5cache.bin";

extern void CleanCacheFiles(const char *pszRootPath, bool cleanEmptyFolders);
