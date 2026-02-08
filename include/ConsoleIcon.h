//=====================================================================================================================================================================================================
// ConsoleIcon
//
// A class to set and then reset the icon for a console window
//
//=====================================================================================================================================================================================================
class ConsoleIcon
{
public:
	ConsoleIcon(size_t IconId)
	{
		m_hWnd = GetConsoleWindow();

		m_hIconOldBig = (HICON)SendMessage(m_hWnd, WM_GETICON, ICON_BIG, 0);
		m_hIconOldSmall = (HICON)SendMessage(m_hWnd, WM_GETICON, ICON_SMALL, 0);

		m_hIcon = LoadIcon(GetModuleHandle(nullptr), MAKEINTRESOURCE(IconId));

		SendMessage(m_hWnd, WM_SETICON, ICON_BIG, (LPARAM)m_hIcon);
		SendMessage(m_hWnd, WM_SETICON, ICON_SMALL, (LPARAM)m_hIcon);
	}

	~ConsoleIcon()
	{
		SendMessage(m_hWnd, WM_SETICON, ICON_BIG, (LPARAM)m_hIconOldBig);
		SendMessage(m_hWnd, WM_SETICON, ICON_SMALL, (LPARAM)m_hIconOldSmall);
	}

private:
	HWND	m_hWnd;
	HICON	m_hIcon;
	HICON	m_hIconOldBig;
	HICON	m_hIconOldSmall;
};


