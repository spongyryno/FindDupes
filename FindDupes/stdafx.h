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
#include <stdio.h>
#include <vector>
#include <algorithm>
#include <unordered_map>
#include <assert.h>
#include <string>
#include <functional>
#include <set>
#include <regex>
#include <string>
#include <filesystem>

// other
#include <conio.h>
#include <intrin.h>