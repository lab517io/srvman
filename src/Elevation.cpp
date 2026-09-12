// Elevation.cpp : running the service manager with administrator rights
//
// See Elevation.h for what this is for and why the elevated instance has to be a new
// process rather than a promoted one.
//
#include "stdafx.h"

#include "Elevation.h"

#include <bzscore/status.h>
#include <shellapi.h>

using namespace BazisLib;

static HBITMAP s_hShieldBitmap;

void Elevation::Shutdown()
{
	if (s_hShieldBitmap)
	{
		::DeleteObject(s_hShieldBitmap);
		s_hShieldBitmap = NULL;
	}
}

bool Elevation::IsElevated()
{
	//Cached: a token cannot gain the administrators group while the process runs, which
	//is the whole reason elevating means starting a new process.
	static int s_Elevated = -1;
	if (s_Elevated < 0)
	{
		s_Elevated = 0;

		HANDLE hToken = NULL;
		if (::OpenProcessToken(::GetCurrentProcess(), TOKEN_QUERY, &hToken))
		{
			//TokenIsElevated is false for the filtered token UAC hands an administrator,
			//and true for anyone holding a full one - including the case where UAC is
			//switched off altogether and there is nothing to elevate to.
			TOKEN_ELEVATION elevation = {0,};
			DWORD cbReturned = 0;
			if (::GetTokenInformation(hToken, TokenElevation, &elevation, sizeof(elevation), &cbReturned))
				s_Elevated = elevation.TokenIsElevated ? 1 : 0;
			::CloseHandle(hToken);
		}
	}
	return s_Elevated != 0;
}

void Elevation::SetButtonShield(HWND hwndButton)
{
	if (hwndButton)
		::SendMessage(hwndButton, BCM_SETSHIELD, 0, TRUE);
}

//! Renders the stock shield icon into a bitmap a menu item can carry.
/*! DrawIconEx() alpha-blends the icon onto the DIB, which starts out zeroed and therefore
	fully transparent, so what lands in the bitmap is already premultiplied - the form
	32bpp menu bitmaps are blended from. Premultiplying it again here would darken it. */
static HBITMAP CreateShieldBitmap()
{
	//SHGSI_SMALLICON asks for the SM_CXSMICON size, which Windows has already scaled to
	//the DPI the process declared in the manifest.
	SHSTOCKICONINFO sii = {sizeof(sii),};
	if (FAILED(::SHGetStockIconInfo(SIID_SHIELD, SHGSI_ICON | SHGSI_SMALLICON, &sii)))
		return NULL;

	const int cx = ::GetSystemMetrics(SM_CXSMICON), cy = ::GetSystemMetrics(SM_CYSMICON);

	BITMAPINFO bi = {0,};
	bi.bmiHeader.biSize = sizeof(bi.bmiHeader);
	bi.bmiHeader.biWidth = cx;
	bi.bmiHeader.biHeight = -cy;	//top-down, so the rows are in the order DrawIconEx writes them
	bi.bmiHeader.biPlanes = 1;
	bi.bmiHeader.biBitCount = 32;
	bi.bmiHeader.biCompression = BI_RGB;

	void *pBits = NULL;
	HBITMAP hBitmap = ::CreateDIBSection(NULL, &bi, DIB_RGB_COLORS, &pBits, NULL, 0);
	if (hBitmap)
	{
		HDC hdc = ::CreateCompatibleDC(NULL);
		if (hdc)
		{
			HGDIOBJ hOldBitmap = ::SelectObject(hdc, hBitmap);
			::DrawIconEx(hdc, 0, 0, sii.hIcon, cx, cy, 0, NULL, DI_NORMAL);
			::SelectObject(hdc, hOldBitmap);
			::DeleteDC(hdc);
		}
	}

	::DestroyIcon(sii.hIcon);
	return hBitmap;
}

void Elevation::SetMenuItemShield(HMENU hMenu, UINT nCommandID)
{
	if (!s_hShieldBitmap)
		s_hShieldBitmap = CreateShieldBitmap();
	if (!s_hShieldBitmap)
		return;		//no shield to show; the item's text says the same thing

	MENUITEMINFO mii = {sizeof(mii),};
	mii.fMask = MIIM_BITMAP;
	mii.hbmpItem = s_hShieldBitmap;
	::SetMenuItemInfo(hMenu, nCommandID, FALSE, &mii);
}

bool Elevation::Relaunch(HWND hwndParent, LPCTSTR lpArguments)
{
	TCHAR tszExecutable[MAX_PATH * 4];
	DWORD done = ::GetModuleFileName(NULL, tszExecutable, __countof(tszExecutable));
	if (!done || (done >= __countof(tszExecutable)))
		return false;

	SHELLEXECUTEINFO info = {0,};
	info.cbSize = sizeof(info);
	//SEE_MASK_NOASYNC because the caller closes as soon as this returns true, and
	//ShellExecuteEx() must not still be talking to the shell from a thread that is about
	//to go away. SEE_MASK_FLAG_NO_UI keeps the shell's own error box out of the way of
	//the one below, which can say what was being attempted.
	info.fMask = SEE_MASK_NOASYNC | SEE_MASK_FLAG_NO_UI;
	info.hwnd = hwndParent;
	info.lpVerb = _T("runas");
	info.lpFile = tszExecutable;
	info.lpParameters = lpArguments;
	info.nShow = SW_SHOWNORMAL;

	if (::ShellExecuteEx(&info))
		return true;

	//A dismissed prompt is the user saying no, not something to report back to them.
	DWORD err = ::GetLastError();
	if (err != ERROR_CANCELLED)
	{
		ActionStatus status = MAKE_STATUS(ActionStatus::FromWin32Error(err));
		::MessageBox(hwndParent,
			(String(_T("Cannot restart as administrator: ")) + status.GetMostInformativeText()).c_str(),
			_T("Service Manager"), MB_ICONERROR);
	}
	return false;
}
