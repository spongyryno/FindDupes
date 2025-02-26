#include <stdafx.h>
//#include <windows.h>
//#include <stdio.h>
#include "console.h"

namespace console
{

//========================================================================
//========================================================================
HANDLE GetCurrentConsoleOutputHandle(void)
{
	HANDLE hFile = CreateFileW(L"CONOUT$", GENERIC_WRITE | GENERIC_READ, FILE_SHARE_WRITE, NULL, OPEN_EXISTING, NULL, NULL);
	return hFile;
}

//========================================================================
//========================================================================
HANDLE GetCurrentConsoleInputHandle(void)
{
	HANDLE hFile = CreateFileW(L"CONIN$", GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, NULL, NULL);
	return hFile;
}



//========================================================================
//========================================================================
int Locate(int x, int y)
{
	HANDLE	hFileO;

	hFileO = GetCurrentConsoleOutputHandle();

	CONSOLE_SCREEN_BUFFER_INFO	csbi;
	GetConsoleScreenBufferInfo(hFileO, &csbi);

	csbi.dwCursorPosition.X += x;
	csbi.dwCursorPosition.Y += y;

	SetConsoleCursorPosition(hFileO, csbi.dwCursorPosition);

	CloseHandle(hFileO);
	return 0;
}

//========================================================================
//========================================================================
int SetColors(int x)
{
	HANDLE	hFileO;
	WORD	wOldAttributes;

	hFileO = GetCurrentConsoleOutputHandle();

	CONSOLE_SCREEN_BUFFER_INFO	csbi;
	GetConsoleScreenBufferInfo(hFileO, &csbi);
	wOldAttributes = csbi.wAttributes;

	SetConsoleTextAttribute(hFileO, (WORD)x);

	CloseHandle(hFileO);
	return wOldAttributes;
}

//========================================================================
//========================================================================
int GetColors(void)
{
	HANDLE	hFileO;
	WORD	wOldAttributes;

	hFileO = GetCurrentConsoleOutputHandle();

	CONSOLE_SCREEN_BUFFER_INFO	csbi;
	GetConsoleScreenBufferInfo(hFileO, &csbi);
	wOldAttributes = csbi.wAttributes;

	CloseHandle(hFileO);
	return wOldAttributes;
}


//========================================================================
//========================================================================
void FlipColors(void)
{
	HANDLE	hFileO;
	WORD	wOldAttributes;
	WORD	wNewAttributes;

	hFileO = GetCurrentConsoleOutputHandle();

	CONSOLE_SCREEN_BUFFER_INFO	csbi;
	GetConsoleScreenBufferInfo(hFileO, &csbi);
	wOldAttributes = csbi.wAttributes;
	wNewAttributes = (wOldAttributes & 0xFF00) | ((wOldAttributes & 0x000F) << 4) | ((wOldAttributes & 0x00F0) >> 4);

	//printf("\nOld: %04X\n", wOldAttributes);
	//printf("New: %04X\n", wNewAttributes);

	SetConsoleTextAttribute(hFileO, wNewAttributes);

	CloseHandle(hFileO);
}

}

