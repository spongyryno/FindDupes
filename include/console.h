#pragma once

namespace console
{
extern int Locate(int x, int y);
extern int SetColors(int c);
extern int GetColors(void);
extern void FlipColors(void);
extern HANDLE GetCurrentConsoleOutputHandle(void);
extern HANDLE GetCurrentConsoleInputHandle(void);


#define MAKE_CC_COLOR(f, b) ((f)|((b)<<4))

#define CC_BLACK				 0
#define CC_DK_BLUE				 1
#define CC_GREEN				 2
#define CC_TEAL					 3
#define CC_DK_RED				 4
#define CC_PURPLE 				 5
#define CC_TAN					 6
#define CC_WHITE				 7
#define CC_GRAY					 8
#define CC_BLUE					 9
#define CC_LT_GREEN				10
#define CC_CYAN					11
#define CC_RED					12
#define CC_MAGENTA				13
#define CC_YELLOW				14
#define CC_BR_WHITE				15
}

