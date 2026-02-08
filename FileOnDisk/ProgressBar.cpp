#include "stdafx.h"
#include "progressbar.h"
#include "dprintf.h"

//=====================================================================================================================================================================================================
// debugging
//=====================================================================================================================================================================================================
size_t ___i = 0;



//=====================================================================================================================================================================================================
//=====================================================================================================================================================================================================
#define MAKE_CC_COLOR(f, b) ((f)|((b)<<4))

constexpr long CC_BLACK		= 0;
constexpr long CC_DK_BLUE	= 1;
constexpr long CC_GREEN		= 2;
constexpr long CC_TEAL		= 3;
constexpr long CC_DK_RED	= 4;
constexpr long CC_PURPLE 	= 5;
constexpr long CC_TAN		= 6;
constexpr long CC_WHITE		= 7;
constexpr long CC_GRAY		= 8;
constexpr long CC_BLUE		= 9;
constexpr long CC_LT_GREEN	= 10;
constexpr long CC_CYAN		= 11;
constexpr long CC_RED		= 12;
constexpr long CC_MAGENTA	= 13;
constexpr long CC_YELLOW	= 14;
constexpr long CC_BR_WHITE	= 15;


//=====================================================================================================================================================================================================
//=====================================================================================================================================================================================================
class ProgressBarImpl
{
public:
	ProgressBarImpl()
		:m_initialized(false)
		,m_level(0)
		,m_csbi{0}
		,m_pct(0.0)
		,m_bufferline(0)
		,m_windowline(0)
		,m_hFileO(nullptr)
	{
		Initialize();
	}

	~ProgressBarImpl()
	{
		Close();
	}

	void Initialize();
	void Close();
	void Update(int dist, int total);
	void Update(long dist, long total);
	void Update(long long dist, long long total);
	void Update(double pct);
	void Update();

private:
	static HANDLE GetCurrentConsoleOutputHandle();
	static HANDLE GetCurrentConsoleInputHandle();
	int Locate(int x, int y) const;
	int SetColors(int x) const;
	int GetColors() const;
	void FlipColors() const;
	void CaptureBackText(SHORT windowline);
	void RestoreBackText();
	void WriteProgressBar(SHORT windowline);

private:
	bool								m_initialized;
	int									m_level;
	CONSOLE_SCREEN_BUFFER_INFO			m_csbi;
	double								m_pct;
	HANDLE								m_hFileO;

private:
	SHORT								m_bufferline; // what line in the buffer that the progress bar is on
	SHORT								m_windowline; // what line in the window that the progress bar is on
	SHORT								m_width;
	std::vector<CHAR_INFO>				m_progress;
	std::vector<CHAR_INFO>				m_backtext;

private:
	static int							ms_numProgressBars;
};


//=====================================================================================================================================================================================================
//=====================================================================================================================================================================================================
void ProgressBarImpl::CaptureBackText(SHORT windowline)
{
	m_windowline = windowline;
	m_bufferline = m_csbi.srWindow.Top + windowline;

	m_width = static_cast<SHORT>(m_csbi.srWindow.Right - m_csbi.srWindow.Left);
	m_progress.resize(m_width);
	m_backtext.resize(m_width);

	COORD start = { m_csbi.srWindow.Left + 0, m_csbi.srWindow.Top + 0 };

	COORD dwBufferSize{ m_width, 1 };
	COORD dwBufferCoord{ 0, 0 };
	SMALL_RECT region{ 0 };

	region.Left = 0;
	region.Right = m_width;
	region.Top = m_bufferline;
	region.Bottom = m_bufferline;

	if (ReadConsoleOutput(m_hFileO, m_backtext.data(), dwBufferSize, dwBufferCoord, &region))
	{
		__nop();
	}
	else
	{
		__nop();
	}
}


//=====================================================================================================================================================================================================
//=====================================================================================================================================================================================================
void ProgressBarImpl::WriteProgressBar(SHORT windowline)
{
	COORD dwBufferSize{ (short)m_progress.size(),1 };
	COORD dwBufferCoord{ 0, 0 };
	SMALL_RECT region{ 0 };

	region.Left = 0;
	region.Right = static_cast<SHORT>(m_progress.size()-1);
	region.Top = m_bufferline;
	region.Bottom = m_bufferline;

	if (WriteConsoleOutput(m_hFileO, m_progress.data(), dwBufferSize, dwBufferCoord, &region))
	{
		__nop();
	}
	else
	{
		__nop();
	}
}


//=====================================================================================================================================================================================================
//=====================================================================================================================================================================================================
void ProgressBarImpl::RestoreBackText()
{
	COORD dwBufferSize{ (short)m_backtext.size(),1 };
	COORD dwBufferCoord{ 0, 0 };
	SMALL_RECT region{ 0 };

	region.Left = 0;
	region.Right = static_cast<SHORT>(m_backtext.size()-1);
	region.Top = m_bufferline;
	region.Bottom = m_bufferline;

	if (WriteConsoleOutput(m_hFileO, m_backtext.data(), dwBufferSize, dwBufferCoord, &region))
	{
		__nop();
	}
	else
	{
		__nop();
	}
}


//=====================================================================================================================================================================================================
//=====================================================================================================================================================================================================
int ProgressBarImpl::ms_numProgressBars = 0;


//=====================================================================================================================================================================================================
//=====================================================================================================================================================================================================
HANDLE ProgressBarImpl::GetCurrentConsoleOutputHandle(void)
{
	HANDLE hFile = CreateFileW(L"CONOUT$", GENERIC_WRITE | GENERIC_READ, FILE_SHARE_WRITE, NULL, OPEN_EXISTING, NULL, NULL);
	return hFile;
}


//=====================================================================================================================================================================================================
//=====================================================================================================================================================================================================
HANDLE ProgressBarImpl::GetCurrentConsoleInputHandle(void)
{
	HANDLE hFile = CreateFileW(L"CONIN$", GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, NULL, NULL);
	return hFile;
}


//=====================================================================================================================================================================================================
//=====================================================================================================================================================================================================
int ProgressBarImpl::Locate(int x, int y) const
{
	CONSOLE_SCREEN_BUFFER_INFO	csbi;
	GetConsoleScreenBufferInfo(m_hFileO, &csbi);

	csbi.dwCursorPosition.X += x;
	csbi.dwCursorPosition.Y += y;

	SetConsoleCursorPosition(m_hFileO, csbi.dwCursorPosition);
	return 0;
}

//=====================================================================================================================================================================================================
//=====================================================================================================================================================================================================
int ProgressBarImpl::SetColors(int x) const
{
	WORD	wOldAttributes;

	CONSOLE_SCREEN_BUFFER_INFO	csbi;
	GetConsoleScreenBufferInfo(m_hFileO, &csbi);
	wOldAttributes = csbi.wAttributes;

	SetConsoleTextAttribute(m_hFileO, (WORD)x);

	return wOldAttributes;
}


//=====================================================================================================================================================================================================
//=====================================================================================================================================================================================================
int ProgressBarImpl::GetColors() const
{
	WORD	wOldAttributes;

	CONSOLE_SCREEN_BUFFER_INFO	csbi;
	GetConsoleScreenBufferInfo(m_hFileO, &csbi);
	wOldAttributes = csbi.wAttributes;

	return wOldAttributes;
}


//=====================================================================================================================================================================================================
//=====================================================================================================================================================================================================
void ProgressBarImpl::FlipColors() const
{
	WORD	wOldAttributes;
	WORD	wNewAttributes;

	CONSOLE_SCREEN_BUFFER_INFO	csbi;
	GetConsoleScreenBufferInfo(m_hFileO, &csbi);
	wOldAttributes = csbi.wAttributes;
	wNewAttributes = (wOldAttributes & 0xFF00) | ((wOldAttributes & 0x000F) << 4) | ((wOldAttributes & 0x00F0) >> 4);

	SetConsoleTextAttribute(m_hFileO, wNewAttributes);
}


//=====================================================================================================================================================================================================
//=====================================================================================================================================================================================================
void ProgressBarImpl::Initialize()
{
	if (!m_initialized)
	{
		m_hFileO = GetCurrentConsoleOutputHandle();
		m_level = ms_numProgressBars;
		++ms_numProgressBars;

		if (m_hFileO && m_hFileO != INVALID_HANDLE_VALUE)
		{
		}
		else
		{
			throw std::runtime_error("test");
		}

		GetConsoleScreenBufferInfo(m_hFileO, &m_csbi);
		m_initialized = true;

		CaptureBackText(2 + 2 * m_level);
	}
}


//=====================================================================================================================================================================================================
//=====================================================================================================================================================================================================
void ProgressBarImpl::Close()
{
	if (m_initialized)
	{
		RestoreBackText();
		--ms_numProgressBars;
		m_initialized = false;
	}

	if (m_hFileO)
	{
		if (m_hFileO != INVALID_HANDLE_VALUE)
		{
			CloseHandle(m_hFileO);
			m_hFileO = nullptr;
		}
	}
}


//=====================================================================================================================================================================================================
//=====================================================================================================================================================================================================
void ProgressBarImpl::Update(int dist, int total)
{
	m_pct = dist / ((double)total);
	Update();
}


//=====================================================================================================================================================================================================
//=====================================================================================================================================================================================================
void ProgressBarImpl::Update(long dist, long total)
{
	m_pct = dist / ((double)total);
	Update();
}


//=====================================================================================================================================================================================================
//=====================================================================================================================================================================================================
void ProgressBarImpl::Update(long long dist, long long total)
{
	m_pct = dist / ((double)total);
	Update();
}


//=====================================================================================================================================================================================================
//=====================================================================================================================================================================================================
void ProgressBarImpl::Update(double pct)
{
	m_pct = pct * 0.01;
	Update();
}

//=====================================================================================================================================================================================================
//=====================================================================================================================================================================================================
void ProgressBarImpl::Update()
{
	short line = 2 + m_level * 2;

	CONSOLE_SCREEN_BUFFER_INFO oldcsbi = m_csbi;
	GetConsoleScreenBufferInfo(m_hFileO, &m_csbi);

#ifdef _DEBUG
	dprintf("%6d: (%3d,%3d) ((%3d,%3d)-(%3d,%3d))  (%3d,%3d) ((%3d,%3d)-(%3d,%3d))\n", ___i,
			m_csbi.dwSize.X, m_csbi.dwSize.Y, m_csbi.srWindow.Left, m_csbi.srWindow.Top, m_csbi.srWindow.Right, m_csbi.srWindow.Bottom,
			oldcsbi.dwSize.X, oldcsbi.dwSize.Y, oldcsbi.srWindow.Left, oldcsbi.srWindow.Top, oldcsbi.srWindow.Right, oldcsbi.srWindow.Bottom);
#endif

	if ((m_csbi.srWindow.Top != oldcsbi.srWindow.Top) || (m_csbi.srWindow.Right != oldcsbi.srWindow.Right))
	{
		RestoreBackText();
		CaptureBackText(line);
	}

	for (auto& c : m_progress)
	{
		c.Attributes = MAKE_CC_COLOR(CC_YELLOW, CC_BLACK);
	}

	m_progress[0].Char.UnicodeChar = L'[';

	size_t i0 = 1;
	size_t i2 = m_width - 1;
	size_t i1 = i0 + (int)((i2-i0) * m_pct);

	for (size_t i = i0; i < i2; ++i)
	{
		if (i < i1)
		{
			m_progress[i].Char.UnicodeChar = L'=';
		}
		else
		{
			m_progress[i].Char.UnicodeChar = L' ';
		}
	}

	m_progress[m_width-1].Char.UnicodeChar = L']';

	CHAR_INFO *p = &m_progress[(m_width>>1)-2];
	int pctInt = static_cast<int>(m_pct * 1000);
	int pctIntDig0 = (pctInt / 1000) % 10;
	int pctIntDig1 = (pctInt / 100) % 10;
	int pctIntDig2 = (pctInt / 10) % 10;
	int pctIntDig3 = (pctInt / 1) % 10;
	bool n = false;

	if (n || pctIntDig0 != 0)
	{
		p[0].Char.UnicodeChar = L'0' + pctIntDig0;
		n = true;
	}

	if (n || pctIntDig1 != 0)
	{
		p[1].Char.UnicodeChar = L'0' + pctIntDig1;
		n = true;
	}

	n = true;

	if (n || pctIntDig2 != 0)
	{
		p[2].Char.UnicodeChar = L'0' + pctIntDig2;
		n = true;
	}

	p[3].Char.UnicodeChar = L'.';

	if (n || pctIntDig3 != 0)
	{
		p[4].Char.UnicodeChar = L'0' + pctIntDig3;
		n = true;
	}

	p[5].Char.UnicodeChar = L'%';

	WriteProgressBar(line);

#ifdef _DEBUG
	CONSOLE_SCREEN_BUFFER_INFO new_csbi;
	GetConsoleScreenBufferInfo(m_hFileO, &new_csbi);
	dprintf("        (%3d,%3d) ((%3d,%3d)-(%3d,%3d))\n",
			new_csbi.dwSize.X, new_csbi.dwSize.Y, new_csbi.srWindow.Left, new_csbi.srWindow.Top, new_csbi.srWindow.Right, new_csbi.srWindow.Bottom);

	if (new_csbi.srWindow.Top != m_csbi.srWindow.Top)
	{
		dprintf("====================================================================================================\n");
	}
#endif

	return;
}


//=====================================================================================================================================================================================================
//=====================================================================================================================================================================================================
ProgressBar::ProgressBar()
{
	this->_ = std::make_unique<ProgressBarImpl>();
}

//=====================================================================================================================================================================================================
//=====================================================================================================================================================================================================
ProgressBar::~ProgressBar()
{
}

//=====================================================================================================================================================================================================
//=====================================================================================================================================================================================================
void ProgressBar::Update(int dist, int total)
{
	this->_->Update(dist, total);
}

//=====================================================================================================================================================================================================
//=====================================================================================================================================================================================================
void ProgressBar::Update(long dist, long total)
{
	this->_->Update(dist, total);
}

//=====================================================================================================================================================================================================
//=====================================================================================================================================================================================================
void ProgressBar::Update(long long dist, long long total)
{
	this->_->Update(dist, total);
}

//=====================================================================================================================================================================================================
//=====================================================================================================================================================================================================
void ProgressBar::Update(double pct)
{
	this->_->Update(pct);
}



