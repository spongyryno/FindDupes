// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently
//

#pragma once

#include "targetver.h"

#ifdef _DEBUG
#define _CRTDBG_MAP_ALLOC
#include <stdlib.h>
#include <crtdbg.h>
#endif

// windows stuff
#include <tchar.h>
#include <Windows.h>
#include <Shlobj.h>

// std C++ stuff
#include <algorithm>
#include <assert.h>
#include <filesystem>
#include <functional>
#include <mutex>
#include <regex>
#include <set>
#include <stdio.h>
#include <string>
#include <unordered_map>
#include <vector>

// other
#include <conio.h>
#include <intrin.h>