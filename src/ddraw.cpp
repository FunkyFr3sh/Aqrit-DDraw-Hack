
#define CINTERFACE 
#include <windows.h>
#include <windowsx.h>
#include <ddraw.h>
#include <stdint.h>
#include <stdio.h>
#include <time.h>
#include "hook.h"

void HookFonts(void);
void MouseLock();
void ToScreen(BOOL obsHack);

struct {
	BITMAPINFOHEADER bmiHeader;
	RGBQUAD bmiColors[256]; 
} bmi;

#define IDT_TIMER_OBS_REDRAW 541282468

WNDPROC OrgWndProc;
BOOL Fullscreen = TRUE;
BOOL WindowedFullscreen = FALSE;
BOOL MaintainAspectRatio = FALSE;
BOOL AlwaysOnTop = FALSE;
BOOL ShowWindowFrame = TRUE;
BOOL FullscreenFailed = FALSE;
BOOL IgnoreAltEnter = FALSE;
BOOL AdjustMouseSensitivity = TRUE;
BOOL SaveSettings = TRUE;
int MaximizeScale = 1;
int OverlayFPS = 5;
BOOL IsResized = FALSE;
BOOL MouseLocked;
BOOL BnetActive;
RECT WindowRect;
HWND hwnd_main;
void* pvBmpBits;
HDC hdc_offscreen;
RECT BnetPos = { -32000, -32000 };
RECT BnetWindowRect;
const LONG OriginalWidth = 640;
const LONG OriginalHeight = 480;
float UnscaleW = 1.0f;
float UnscaleH = 1.0f;
LONG CurrentWidth = 640;
LONG CurrentHeight = 480;
LONG CurrentX = -32000;
LONG CurrentY = -32000;
LONG RenderX;
LONG RenderY;
HBITMAP hOldBitmap; // for cleanup
char SettingsIniPath[] = ".\\war2_ddraw.ini";

WNDPROC ButtonWndProc_original;

extern const DWORD dd_vtbl[];
extern const DWORD dds_vtbl[];
extern const DWORD ddp_vtbl[];
const DWORD* const IDDraw = dd_vtbl;
const DWORD* const IDDSurf = dds_vtbl;
const DWORD* const IDDPal = ddp_vtbl;

// real ddraw for fullscreen
IDirectDraw* ddraw;
IDirectDrawSurface* dds_primary = NULL;


BOOL WINAPI fake_ClientToScreen(HWND hWnd, LPPOINT lpPoint)
{
	if (hwnd_main != hWnd)
		return ClientToScreen(hWnd, lpPoint) && ScreenToClient(hwnd_main, lpPoint);

	return TRUE;
}

BOOL WINAPI fake_ScreenToClient(HWND hWnd, LPPOINT lpPoint)
{
	if (hwnd_main != hWnd)
		return ClientToScreen(hwnd_main, lpPoint) && ScreenToClient(hWnd, lpPoint);

	return TRUE;
}

BOOL WINAPI fake_GetWindowRect(HWND hWnd, LPRECT lpRect)
{
	if (hwnd_main != hWnd)
		return GetWindowRect(hWnd, lpRect) && MapWindowPoints(HWND_DESKTOP, hwnd_main, (LPPOINT)lpRect, 2);

	return GetWindowRect(hWnd, lpRect);
}

BOOL WINAPI fake_EnableWindow(HWND hWnd, BOOL bEnable)
{
	if (hWnd == hwnd_main)
	{
		return FALSE;
	}

	return EnableWindow(hWnd, bEnable);
}

BOOL WINAPI fake_DestroyWindow(HWND hWnd)
{
	BOOL result = DestroyWindow(hWnd);

	if (hWnd != hwnd_main && BnetActive && !FindWindowEx(HWND_DESKTOP, NULL, "SDlgDialog", NULL))
	{
		if (OverlayFPS > 0 && OverlayFPS <= 1000)
			KillTimer(hwnd_main, IDT_TIMER_OBS_REDRAW);

		BnetActive = FALSE;

		if (!Fullscreen)
		{
			GetWindowRect(hwnd_main, &BnetPos);

			int width = BnetWindowRect.right - BnetWindowRect.left;
			int height = BnetWindowRect.bottom - BnetWindowRect.top;

			UINT flags = width != OriginalWidth || height != OriginalHeight ? 0 : SWP_NOMOVE;

			if (WindowedFullscreen)
				SetWindowLong(
					hwnd_main,
					GWL_STYLE, 
					GetWindowLong(hwnd_main, GWL_STYLE) & ~(WS_CAPTION | WS_THICKFRAME | WS_MINIMIZE | WS_MAXIMIZE | WS_SYSMENU));

			SetWindowPos(
				hwnd_main,
				AlwaysOnTop ? HWND_TOPMOST : HWND_NOTOPMOST,
				WindowRect.left,
				WindowRect.top,
				WindowRect.right - WindowRect.left,
				WindowRect.bottom - WindowRect.top,
				flags);

			if (WindowedFullscreen)
				MouseLock();
		}
	}

	return result;
}

HWND WINAPI fake_CreateWindowExA(
	DWORD dwExStyle, LPCSTR lpClassName, LPCSTR lpWindowName, DWORD dwStyle, int X, int Y,
	int nWidth, int nHeight, HWND hWndParent, HMENU hMenu, HINSTANCE hInstance, LPVOID lpParam)
{
	if (_strcmpi(lpClassName, "SDlgDialog") == 0)
	{
		if (!BnetActive)
		{
			BnetActive = TRUE;

			if (OverlayFPS > 0 && OverlayFPS <= 1000)
				SetTimer(hwnd_main, IDT_TIMER_OBS_REDRAW, 1000 / OverlayFPS, (TIMERPROC)NULL);

			if (!Fullscreen)
			{
				GetWindowRect(hwnd_main, &WindowRect);

				if (WindowedFullscreen)
					SetWindowLong(hwnd_main, GWL_STYLE, GetWindowLong(hwnd_main, GWL_STYLE) | WS_OVERLAPPEDWINDOW);

				RECT rc = { 0, 0, OriginalWidth, OriginalHeight };
				AdjustWindowRect(&rc, GetWindowLong(hwnd_main, GWL_STYLE), FALSE);

				GetClientRect(hwnd_main, &BnetWindowRect);
				MapWindowPoints(hwnd_main, HWND_DESKTOP, (LPPOINT)&BnetWindowRect, 2);

				int width = BnetWindowRect.right - BnetWindowRect.left;
				int height = BnetWindowRect.bottom - BnetWindowRect.top;

				UINT flags = width != OriginalWidth || height != OriginalHeight ? 0 : SWP_NOMOVE;

				if (BnetPos.left == -32000 || BnetPos.top == -32000)
				{
					BnetPos.left = (GetSystemMetrics(SM_CXSCREEN) / 2) - (OriginalWidth / 2);
					BnetPos.top = (GetSystemMetrics(SM_CYSCREEN) / 2) - (OriginalHeight / 2);
				}

				SetWindowPos(
					hwnd_main,
					AlwaysOnTop ? HWND_TOPMOST : HWND_NOTOPMOST,
					BnetPos.left,
					BnetPos.top,
					rc.right - rc.left,
					rc.bottom - rc.top,
					flags);
			}
		}

		POINT pt = { 0, 0 };
		ClientToScreen(hwnd_main, &pt);

		X += pt.x;
		Y += pt.y;
	}

	return CreateWindowExA(
		dwExStyle,
		lpClassName,
		lpWindowName,
		dwStyle,
		X,
		Y,
		nWidth,
		nHeight,
		hWndParent,
		hMenu,
		hInstance,
		lpParam);
}

DWORD GetString(LPCSTR key, LPCSTR defaultValue, LPSTR outString, DWORD outSize)
{
	return GetPrivateProfileStringA("ddraw", key, defaultValue, outString, outSize, SettingsIniPath);
}

BOOL GetBool(LPCSTR key, BOOL defaultValue)
{
	char value[8];
	GetString(key, defaultValue ? "Yes" : "No", value, sizeof(value));

	return (_stricmp(value, "yes") == 0 || _stricmp(value, "true") == 0 || _stricmp(value, "1") == 0);
}

int GetInt(LPCSTR key, int defaultValue)
{
	char defvalue[16];
	_snprintf(defvalue, sizeof(defvalue), "%d", defaultValue);

	char value[16];
	GetString(key, defvalue, value, sizeof(value));

	return atoi(value);
}

BOOL UnadjustWindowRectEx(LPRECT prc, DWORD dwStyle, BOOL fMenu, DWORD dwExStyle)
{
	RECT rc;
	SetRectEmpty(&rc);

	BOOL fRc = AdjustWindowRectEx(&rc, dwStyle, fMenu, dwExStyle);
	if (fRc)
	{
		prc->left -= rc.left;
		prc->top -= rc.top;
		prc->right -= rc.right;
		prc->bottom -= rc.bottom;
	}

	return fRc;
}

void MouseLock()
{
	RECT rc = { 
		AdjustMouseSensitivity ? RenderX : 0,
		AdjustMouseSensitivity ? RenderY : 0,
		AdjustMouseSensitivity ? CurrentWidth - RenderX : OriginalWidth, 
		AdjustMouseSensitivity ? CurrentHeight - RenderY : OriginalHeight 
	};

	POINT pt = { rc.left, rc.top };
	POINT pt2 = { rc.right, rc.bottom };
	ClientToScreen(hwnd_main, &pt);
	ClientToScreen(hwnd_main, &pt2);

	SetRect(&rc, pt.x, pt.y, pt2.x, pt2.y);
	ClipCursor(&rc);

	MouseLocked = TRUE;
}

void MouseUnlock()
{
	MouseLocked = FALSE;

	ClipCursor(NULL);
}

void UpdateBnetPos(int oldX, int oldY, int newX, int newY)
{
	POINT pt = { 0, 0 };
	ClientToScreen(hwnd_main, &pt);

	RECT mainrc;
	SetRect(&mainrc, pt.x, pt.y, pt.x + OriginalWidth, pt.y + OriginalHeight);

	int adjY = 0;
	int adjX = 0;

	HWND hWnd = FindWindowEx(HWND_DESKTOP, NULL, "SDlgDialog", NULL);

	while (hWnd != NULL)
	{
		RECT rc;
		GetWindowRect(hWnd, &rc);

		OffsetRect(&rc, newX - oldX, newY - oldY);

		SetWindowPos(
			hWnd,
			0,
			rc.left,
			rc.top,
			0,
			0,
			SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);

		if (rc.bottom > mainrc.bottom && abs(mainrc.bottom - rc.bottom) > abs(adjY))
			adjY = mainrc.bottom - rc.bottom;
		else if (rc.top < mainrc.top && abs(mainrc.top - rc.top) > abs(adjY))
			adjY = mainrc.top - rc.top;

		if (rc.right > mainrc.right && abs(mainrc.right - rc.right) > abs(adjX))
			adjX = mainrc.right - rc.right;
		else if (rc.left < mainrc.left && abs(mainrc.left - rc.left) > abs(adjX))
			adjX = mainrc.left - rc.left;

		hWnd = FindWindowEx(HWND_DESKTOP, hWnd, "SDlgDialog", NULL);
	}

	if (adjX || adjY)
	{
		HWND hWnd = FindWindowEx(HWND_DESKTOP, NULL, "SDlgDialog", NULL);

		while (hWnd != NULL)
		{
			RECT rc;
			GetWindowRect(hWnd, &rc);

			OffsetRect(&rc, adjX, adjY);

			SetWindowPos(
				hWnd,
				0,
				rc.left,
				rc.top,
				0,
				0,
				SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);

			hWnd = FindWindowEx(HWND_DESKTOP, hWnd, "SDlgDialog", NULL);
		}
	}
}

void ToggleFullscreen(BOOL fakeFullscreen)
{
	if (Fullscreen || fakeFullscreen)
	{
		Fullscreen = FALSE;

		MouseUnlock();

		if (ddraw)
		{
			ddraw->lpVtbl->SetCooperativeLevel(ddraw, hwnd_main, DDSCL_NORMAL);
			ChangeDisplaySettingsA(NULL, 0);
			//ddraw->lpVtbl->RestoreDisplayMode(ddraw);
		}

		if (ShowWindowFrame && !fakeFullscreen)
		{
			SetWindowLong(hwnd_main, GWL_STYLE, GetWindowLong(hwnd_main, GWL_STYLE) | WS_OVERLAPPEDWINDOW);
		}
		else
		{
			SetWindowLong(hwnd_main, GWL_STYLE, GetWindowLong(hwnd_main, GWL_STYLE) & ~(WS_CAPTION | WS_THICKFRAME | WS_MINIMIZE | WS_MAXIMIZE | WS_SYSMENU));
		}

		if (!WindowRect.right && !WindowRect.bottom)
		{
			int screenWidth = GetSystemMetrics(SM_CXSCREEN);
			int screenHeight = GetSystemMetrics(SM_CYSCREEN);

			int width = GetInt("Width", OriginalWidth);
			int height = GetInt("Height", OriginalHeight);

			IsResized = width > OriginalWidth || height > OriginalHeight;

			if (fakeFullscreen)
			{
				width = screenWidth;
				height = screenHeight;
				ShowWindowFrame = FALSE;
				AlwaysOnTop = FALSE;
			}

			if (!ShowWindowFrame && width == screenWidth && height == screenHeight)
				WindowedFullscreen = TRUE;

			if (width < OriginalWidth)
				width = OriginalWidth;

			if (height < OriginalHeight)
				height = OriginalHeight;

			LONG x = GetInt("PosX", -32000);
			LONG y = GetInt("PosY", -32000);

			if (x == -32000 || y == -32000)
			{
				x = (screenWidth / 2) - (width / 2);
				y = (screenHeight / 2) - (height / 2);
			}

			if (fakeFullscreen)
			{
				x = 0;
				y = 0;
			}

			WindowRect = { x, y, width + x, height + y };

			AdjustWindowRect(&WindowRect, GetWindowLong(hwnd_main, GWL_STYLE), FALSE);
		}

		CurrentX = CurrentY = 0;

		int width = WindowRect.right - WindowRect.left;
		int height = WindowRect.bottom - WindowRect.top;

		if (BnetActive)
		{
			RECT rc = { 0, 0, OriginalWidth, OriginalHeight };
			AdjustWindowRect(&rc, GetWindowLong(hwnd_main, GWL_STYLE), FALSE);

			width = rc.right - rc.left;
			height = rc.bottom - rc.top;
		}

		SetWindowPos(
			hwnd_main, 
			AlwaysOnTop ? HWND_TOPMOST : HWND_NOTOPMOST,
			WindowRect.left,
			WindowRect.top,
			width,
			height,
			SWP_SHOWWINDOW);


		if (WindowedFullscreen)
			MouseLock();
	}
	else if (ddraw && !WindowedFullscreen)
	{
		Fullscreen = TRUE;

		MouseUnlock();

		if (!BnetActive)
			GetWindowRect(hwnd_main, &WindowRect);

		SetWindowLong(hwnd_main, GWL_STYLE, GetWindowLong(hwnd_main, GWL_STYLE) & ~(WS_CAPTION | WS_THICKFRAME | WS_MINIMIZE | WS_MAXIMIZE | WS_SYSMENU));
		
		SetWindowPos(hwnd_main, HWND_TOPMOST, 0, 0, OriginalWidth, OriginalHeight, SWP_SHOWWINDOW);
		UpdateBnetPos(CurrentX, CurrentY, 0, 0);

		ddraw->lpVtbl->SetCooperativeLevel(ddraw, hwnd_main, DDSCL_FULLSCREEN | DDSCL_EXCLUSIVE);
		ddraw->lpVtbl->SetDisplayMode(ddraw, OriginalWidth, OriginalHeight, 32);

		MouseLock();
	}
}

void ToggleMaximize()
{
	RECT rc;
	if (!BnetActive && SystemParametersInfo(SPI_GETWORKAREA, 0, &rc, 0))
	{
		int width = AlwaysOnTop ? GetSystemMetrics(SM_CXSCREEN) : (rc.right - rc.left);
		int height = AlwaysOnTop ? GetSystemMetrics(SM_CYSCREEN) : (rc.bottom - rc.top);
		int x = AlwaysOnTop ? 0 : rc.left;
		int y = AlwaysOnTop ? 0 : rc.top;

		rc.right = AlwaysOnTop ? width : rc.right;
		rc.bottom = AlwaysOnTop ? height : rc.bottom;
		rc.left = AlwaysOnTop ? x : rc.left;
		rc.top = AlwaysOnTop ? y : rc.top;

		int scale = MaximizeScale;

		if (scale == 1)
		{
			for (int i = 20; i-- > 1;)
			{
				if (width >= OriginalWidth * i && height - 20 >= OriginalHeight * i)
				{
					scale = i;
					break;
				}
			}
		}

		if (scale >= 2 && width >= OriginalWidth * scale && height - 20 >= OriginalHeight * scale)
		{
			rc.top = 0;
			rc.left = 0;
			rc.right = IsResized ? OriginalWidth : OriginalWidth * scale;
			rc.bottom = IsResized ? OriginalHeight : OriginalHeight * scale;

			AdjustWindowRect(&rc, GetWindowLong(hwnd_main, GWL_STYLE), FALSE);
		}
		else if (IsResized)
		{
			rc.top = 0;
			rc.left = 0;
			rc.right = OriginalWidth;
			rc.bottom = OriginalHeight;

			AdjustWindowRect(&rc, GetWindowLong(hwnd_main, GWL_STYLE), FALSE);
		}
		else if (MaintainAspectRatio)
		{
			UnadjustWindowRectEx(&rc, GetWindowLong(hwnd_main, GWL_STYLE), FALSE, GetWindowLong(hwnd_main, GWL_EXSTYLE));

			int w = rc.right - rc.left;
			int h = rc.bottom - rc.top;

			rc.top = 0;
			rc.left = 0;
			rc.right = w;
			rc.bottom = ((float)OriginalHeight / OriginalWidth) * w;

			if (rc.bottom > h)
			{
				rc.right = ((float)rc.right / rc.bottom) * h;
				rc.bottom = h;
			}

			AdjustWindowRect(&rc, GetWindowLong(hwnd_main, GWL_STYLE), FALSE);
		}

		IsResized = !IsResized;

		SetWindowPos(
			hwnd_main,
			AlwaysOnTop ? HWND_TOPMOST : HWND_NOTOPMOST,
			(width / 2) - ((rc.right - rc.left) / 2) + x,
			(height / 2) - ((rc.bottom - rc.top) / 2) + y,
			(rc.right - rc.left),
			(rc.bottom - rc.top),
			SWP_SHOWWINDOW);
	}
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
		case WM_TIMER:
		{
			switch (wParam)
			{
			case IDT_TIMER_OBS_REDRAW:
			{
				ToScreen(TRUE);
				return 0;
			}
			}
			break;
		}
		case WM_NCHITTEST:
		{
			LRESULT result = DefWindowProc(hWnd, uMsg, wParam, lParam);

			if (BnetActive)
			{
				switch (result)
				{
					case HTBOTTOM:
					case HTBOTTOMLEFT:
					case HTBOTTOMRIGHT:
					case HTLEFT:
					case HTRIGHT:
					case HTTOP:
					case HTTOPLEFT:
					case HTTOPRIGHT:
						return HTBORDER;
				}
			}

			return result;
		}
		case WM_SIZING:
		{
			RECT *windowrc = (RECT *)lParam;

			if (!Fullscreen)
			{
				RECT clientrc = { 0 };

				// maintain aspect ratio
				if (MaintainAspectRatio &&
					CopyRect(&clientrc, windowrc) &&
					UnadjustWindowRectEx(&clientrc, GetWindowLong(hWnd, GWL_STYLE), FALSE, GetWindowLong(hWnd, GWL_EXSTYLE)) &&
					SetRect(&clientrc, 0, 0, clientrc.right - clientrc.left, clientrc.bottom - clientrc.top))
				{
					float scaleH = (float)OriginalHeight / OriginalWidth;
					float scaleW = (float)OriginalWidth / OriginalHeight;

					switch (wParam)
					{
						case WMSZ_BOTTOMLEFT:
						case WMSZ_BOTTOMRIGHT:
						case WMSZ_LEFT:
						case WMSZ_RIGHT:
						{
							windowrc->bottom += scaleH * clientrc.right - clientrc.bottom;
							break;
						}
						case WMSZ_TOP:
						case WMSZ_BOTTOM:
						{
							windowrc->right += scaleW * clientrc.bottom - clientrc.right;
							break;
						}
						case WMSZ_TOPRIGHT:
						case WMSZ_TOPLEFT:
						{
							windowrc->top -= scaleH * clientrc.right - clientrc.bottom;
							break;
						}
					}
				}

				//enforce minimum window size
				if (CopyRect(&clientrc, windowrc) &&
					UnadjustWindowRectEx(&clientrc, GetWindowLong(hWnd, GWL_STYLE), FALSE, GetWindowLong(hWnd, GWL_EXSTYLE)) &&
					SetRect(&clientrc, 0, 0, clientrc.right - clientrc.left, clientrc.bottom - clientrc.top))
				{
					if (clientrc.right < OriginalWidth)
					{
						switch (wParam)
						{
							case WMSZ_TOPRIGHT:
							case WMSZ_BOTTOMRIGHT:
							case WMSZ_RIGHT:
							case WMSZ_BOTTOM:
							case WMSZ_TOP:
							{
								windowrc->right += OriginalWidth - clientrc.right;
								break;
							}
							case WMSZ_TOPLEFT:
							case WMSZ_BOTTOMLEFT:
							case WMSZ_LEFT:
							{
								windowrc->left -= OriginalWidth - clientrc.right;
								break;
							}
						}
					}

					if (clientrc.bottom < OriginalHeight)
					{
						switch (wParam)
						{
							case WMSZ_BOTTOMLEFT:
							case WMSZ_BOTTOMRIGHT:
							case WMSZ_BOTTOM:
							case WMSZ_RIGHT:
							case WMSZ_LEFT:
							{
								windowrc->bottom += OriginalHeight - clientrc.bottom;
								break;
							}
							case WMSZ_TOPLEFT:
							case WMSZ_TOPRIGHT:
							case WMSZ_TOP:
							{
								windowrc->top -= OriginalHeight - clientrc.bottom;
								break;
							}
						}
					}
				}

				//save new window position
				if (CopyRect(&clientrc, windowrc) &&
					UnadjustWindowRectEx(&clientrc, GetWindowLong(hWnd, GWL_STYLE), FALSE, GetWindowLong(hWnd, GWL_EXSTYLE)))
				{
					//WindowRect.left = clientrc.left;
					//WindowRect.top = clientrc.top;
					//CurrentWidth = clientrc.right - clientrc.left;
					//CurrentHeight = clientrc.bottom - clientrc.top;
				}

				return TRUE;
			}
			break;
		}
		case WM_SIZE:
		{
			if (wParam == SIZE_RESTORED || wParam == SIZE_MAXIMIZED)
			{
				CurrentWidth = LOWORD(lParam);
				CurrentHeight = HIWORD(lParam);

				UnscaleW = ((float)OriginalWidth / CurrentWidth);
				UnscaleH = ((float)OriginalHeight / CurrentHeight);

				if (MaintainAspectRatio)
				{
					LONG width = CurrentWidth;
					LONG height = ((float)OriginalHeight / OriginalWidth) * CurrentWidth;

					if (height > CurrentHeight)
					{
						width = ((float)width / height) * CurrentHeight;
						height = CurrentHeight;
					}

					RenderY = CurrentHeight / 2 - height / 2;
					RenderX = CurrentWidth / 2 - width / 2;

					UnscaleW = ((float)OriginalWidth / width);
					UnscaleH = ((float)OriginalHeight / height);

					RECT rc = { 0, 0, CurrentWidth, CurrentHeight };

					HDC hdc = GetDC(hwnd_main);
					FillRect(hdc, &rc, (HBRUSH)GetStockObject(BLACK_BRUSH));
					ReleaseDC(hwnd_main, hdc);
				}
			}
			break;
		}
		case WM_MOVE:
		{
			if (!Fullscreen)
			{
				int x = (int)(short)LOWORD(lParam);
				int y = (int)(short)HIWORD(lParam);

				if (x != -32000)
				{
					UpdateBnetPos(CurrentX, CurrentY, x, y);
					CurrentX = x;
				}

				if (y != -32000)
				{
					CurrentY = y;
				}
			}

			break;
		}
		case WM_SETCURSOR:
		{
			// show resize cursor on window borders
			if ((HWND)wParam == hwnd_main)
			{
				WORD message = HIWORD(lParam);

				if (message == WM_MOUSEMOVE)
				{
					static BOOL visible = FALSE;
					WORD htcode = LOWORD(lParam);

					switch (htcode)
					{
						case HTCAPTION:
						case HTMINBUTTON:
						case HTMAXBUTTON:
						case HTCLOSE:
						case HTBOTTOM:
						case HTBOTTOMLEFT:
						case HTBOTTOMRIGHT:
						case HTLEFT:
						case HTRIGHT:
						case HTTOP:
						case HTTOPLEFT:
						case HTTOPRIGHT:
						{
							if (!visible)
							{
								visible = TRUE;
								ShowCursor(TRUE);
							}

							return DefWindowProc(hWnd, uMsg, wParam, lParam);
						}
						default:
						{
							if (visible)
							{
								visible = FALSE;
								ShowCursor(FALSE);
							}
									
							break;
						}
					}
				}
			}

			break;
		}
		case WM_NCLBUTTONDBLCLK:
		{
			ToggleMaximize();
			return 0;
		}
		case WM_NCMOUSELEAVE:
		{
			LONG style = GetWindowLong(hwnd_main, GWL_STYLE);

			if (!(style & WS_MAXIMIZEBOX))
			{
				SetWindowLongA(hwnd_main, GWL_STYLE, style | WS_MAXIMIZEBOX);
			}

			break;
		}
		case WM_SYSCOMMAND:
		{
			if ((wParam & ~0x0F) == SC_MOVE) /* hack: disable aero snap */
			{
				LONG style = GetWindowLong(hwnd_main, GWL_STYLE);

				if ((style & WS_MAXIMIZEBOX))
				{
					SetWindowLongA(hwnd_main, GWL_STYLE, style & ~WS_MAXIMIZEBOX);
				}
			}

			if (wParam == SC_MAXIMIZE)
			{
				ToggleMaximize();
				return 0;
			}
			break;
		}
		case WM_RBUTTONDOWN:
		{
			if (!MouseLocked && !BnetActive)
				MouseLock();

			/* fall through for lParam */
		}
		case WM_LBUTTONUP:
		case WM_RBUTTONUP:
		case WM_MBUTTONUP:
		case WM_XBUTTONDBLCLK:
		case WM_XBUTTONDOWN:
		case WM_XBUTTONUP:
		case WM_MOUSEWHEEL:
		case WM_MOUSEHOVER:
		case WM_LBUTTONDBLCLK:
		case WM_MBUTTONDBLCLK:
		case WM_RBUTTONDBLCLK:
		case WM_LBUTTONDOWN:
		case WM_MBUTTONDOWN:
		case WM_MOUSEMOVE:
		{
			if (AdjustMouseSensitivity)
			{
				int x = ((GET_X_LPARAM(lParam) - RenderX) * UnscaleW);
				int y = ((GET_Y_LPARAM(lParam) - RenderY) * UnscaleH);

				lParam = MAKELPARAM(x, y);
			}

			break;
		}
		case WM_KEYDOWN:
		{
			if (wParam == VK_CONTROL || wParam == VK_TAB)
			{
				if (GetAsyncKeyState(VK_CONTROL) & 0x8000 && GetAsyncKeyState(VK_TAB) & 0x8000)
				{
					if (MouseLocked)
						MouseUnlock();
					else
						MouseLock();

					return 0;
				}
			}
			break;
		}
		case WM_SYSKEYDOWN:
		{
			if (wParam == VK_RETURN)
			{
				if (!IgnoreAltEnter)
				{ 
					ToggleFullscreen(FALSE);
					RedrawWindow(NULL, NULL, NULL, RDW_ERASE | RDW_INVALIDATE | RDW_ALLCHILDREN);
					return 0;
				}
			}

			if (wParam == VK_BACK)
			{
				if (!Fullscreen && !ShowWindowFrame)
				{
					ShowWindow(hwnd_main, SW_MINIMIZE);
					return 0;
				}
			}

			if (wParam == VK_END)
			{
				if (!Fullscreen)
				{
					AlwaysOnTop = !AlwaysOnTop;

					SetWindowPos(
						hwnd_main, 
						AlwaysOnTop ? HWND_TOPMOST : HWND_NOTOPMOST, 
						0, 
						0, 
						0, 
						0, 
						SWP_NOSIZE | SWP_NOMOVE);

					return 0;
				}
			}
			break;
		}
		case WM_ACTIVATEAPP:
		{
			if (!wParam)
			{
				MouseUnlock();
			}

			// keep drawing in windowed mode
			if (!Fullscreen)
				return 0;

			break;
		}
		case WM_ACTIVATE:
		{
			if (wParam == WA_INACTIVE)
			{
				//MouseUnlock();
			}
			else if (wParam == WA_ACTIVE)
			{
				if (Fullscreen || WindowedFullscreen)
					MouseLock();
			}
			break;
		}
	}
	return CallWindowProcA(OrgWndProc, hWnd, uMsg, wParam, lParam);
}

HRESULT GoFullscreen( void )
{
	HMODULE ddraw_dll; 
	DDSURFACEDESC ddsd;
	typedef HRESULT (__stdcall* DIRECTDRAWCREATE)( GUID*, IDirectDraw**, IUnknown* ); 

	// load ddraw.dll from system32 dir
	char szPath[ MAX_PATH ];
	if( GetSystemDirectory( szPath, MAX_PATH - 10 ))
	{
		strcat_s( szPath, "\\ddraw.dll" );
		ddraw_dll = LoadLibrary( szPath );

		if( ddraw_dll != NULL)
		{
			DIRECTDRAWCREATE pfnDirectDrawCreate = (DIRECTDRAWCREATE) GetProcAddress( ddraw_dll, "DirectDrawCreate" );
			if( pfnDirectDrawCreate != NULL )
			{
				if( SUCCEEDED( pfnDirectDrawCreate( (GUID*)0, &ddraw, NULL ) ) )
				{ 
					if (Fullscreen)
					{
						if (SUCCEEDED(ddraw->lpVtbl->SetCooperativeLevel(ddraw, hwnd_main, DDSCL_FULLSCREEN | DDSCL_EXCLUSIVE)))
						{
							if (SUCCEEDED(ddraw->lpVtbl->SetDisplayMode(ddraw, OriginalWidth, OriginalHeight, 32)))
							{
								RtlSecureZeroMemory(&ddsd, sizeof(ddsd));
								ddsd.dwSize = sizeof(ddsd);
								ddsd.dwFlags = DDSD_CAPS;
								ddsd.ddsCaps.dwCaps = DDSCAPS_PRIMARYSURFACE;
								if (SUCCEEDED(ddraw->lpVtbl->CreateSurface(ddraw, &ddsd, &dds_primary, NULL)))
								{
									MouseLock();
									return DD_OK;
								}
								ddraw->lpVtbl->RestoreDisplayMode(ddraw);
							}
							ddraw->lpVtbl->SetCooperativeLevel(ddraw, hwnd_main, DDSCL_NORMAL);
						}
						ddraw->lpVtbl->Release(ddraw);
						ddraw = NULL;
					}
					else
					{
						Fullscreen = TRUE;
						ToggleFullscreen(FALSE);
						return DD_OK;
					}
						
				}
			}
			FreeLibrary( ddraw_dll );
		}
	}

	FullscreenFailed = TRUE;
	ToggleFullscreen(TRUE);
	MouseLock();
	return DDERR_GENERIC;
}

BOOL CheckFullscreen()
{
	if( dds_primary == NULL ) return FALSE;
	if( SUCCEEDED( dds_primary->lpVtbl->IsLost( dds_primary ) ) ) return TRUE;
	if( SUCCEEDED( dds_primary->lpVtbl->Restore( dds_primary ) ) ) return TRUE;
	return FALSE;
}


// HACK // 
// as a work-around for a unkown problem...
// ...I wish to force a redraw when the "Player Profile" screen exits
// however, it is easier to hook a system class ( button ) than a local class ( SDlgDialog )
// so instead we'll force a redraw anytime a button is destroyed.
LRESULT __stdcall ButtonWndProc( HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam )
{
	if( msg == WM_DESTROY ) RedrawWindow( NULL, NULL, NULL, RDW_ERASE | RDW_INVALIDATE | RDW_ALLCHILDREN );
	return ButtonWndProc_original( hwnd, msg, wParam, lParam );
}

void ToScreen(BOOL obsHack)
{
	HDC hdc;
	HWND hwnd;
	RECT rc;
	DWORD* p;
	int i;
	RGBQUAD quad;
	COLORREF clear_color;

	CheckFullscreen();

	hwnd = BnetActive ? FindWindowEx(HWND_DESKTOP, NULL, "SDlgDialog", NULL) : NULL; // detect mixed gdi/ddraw screen
	if( hwnd == NULL ) // in-game (ddraw only)
	{  
		static DWORD lastTick;
		DWORD curTick = timeGetTime();

		if (lastTick + 8 > curTick)
			return;

		lastTick = curTick;

		// simpler/faster blit that also keeps screen shots (mostly) working...
		// no screen flash when ss is taken?
		hdc = GetDC( hwnd_main );
		//BitBlt( hdc, 0, 0, WindowSize.right, WindowSize.bottom, hdc_offscreen, 0, 0, SRCCOPY );

		//SetStretchBltMode(hdc, HALFTONE);

		StretchBlt(
			hdc,
			RenderX,
			RenderY,
			CurrentWidth - (RenderX * 2), 
			CurrentHeight - (RenderY * 2),
			hdc_offscreen,
			0,
			0,
			OriginalWidth,
			OriginalHeight,
			SRCCOPY
		);

		ReleaseDC( hwnd_main, hdc );
		return;
	}
	else if (obsHack)
	{
		fake_GetWindowRect(hwnd, &rc);

		hdc = GetDC(hwnd);
		HDC hdcMain = GetDC(hwnd_main);

		BitBlt(
			hdcMain,
			rc.left,
			rc.top,
			rc.right - rc.left,
			rc.bottom - rc.top,
			hdc,
			0,
			0,
			SRCCOPY);

		ReleaseDC(hwnd_main, hdcMain);
		ReleaseDC(hwnd, hdc);
		return;
	}

	// hijack one of the palette entries to be a clear color :-(
	GetDIBColorTable( hdc_offscreen, 0xFE, 1, &quad );
	clear_color = RGB( quad.rgbRed, quad.rgbGreen, quad.rgbBlue );
	
	hdc = GetDC( hwnd_main );
	GdiTransparentBlt( hdc, 0, 0, OriginalWidth, OriginalHeight, hdc_offscreen, 0, 0, OriginalWidth, OriginalHeight, clear_color );
	ReleaseDC( hwnd_main, hdc );

	// blast it out to all top-level SDlgDialog windows... the realwtf
	do
	{	
		fake_GetWindowRect( hwnd, &rc );
		hdc = GetDCEx( hwnd, NULL, DCX_PARENTCLIP | DCX_CACHE );

		GdiTransparentBlt( 
			hdc, 
			0, 
			0, 
			rc.right - rc.left, 
			rc.bottom - rc.top,
			hdc_offscreen, 
			rc.left, 
			rc.top, 
			rc.right - rc.left, 
			rc.bottom - rc.top,
			clear_color);

		ReleaseDC( hwnd, hdc );
		hwnd = FindWindowEx( HWND_DESKTOP, hwnd, "SDlgDialog", NULL );
	} while( hwnd != NULL );

	// erase ( breaks screen shots, use alt+prtnscr instead )
	p = (DWORD*) pvBmpBits;
	for( i = 0; i < OriginalWidth * OriginalHeight / 4; i++ ) p[i] = 0xFEFEFEFE;

	return;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	HBITMAP hBitmap;
	WNDCLASS wc;
	HINSTANCE hInst;

	if(ul_reason_for_call == DLL_PROCESS_ATTACH )
	{
		DisableThreadLibraryCalls(hModule);

		// create offscreen drawing surface
		bmi.bmiHeader.biSize = sizeof( BITMAPINFOHEADER ); 
		bmi.bmiHeader.biWidth = OriginalWidth;
		bmi.bmiHeader.biHeight = 0 - OriginalHeight;
		bmi.bmiHeader.biPlanes = 1;
		bmi.bmiHeader.biBitCount = 8;
		bmi.bmiHeader.biCompression = BI_RGB;
		bmi.bmiHeader.biSizeImage = 0;
		bmi.bmiHeader.biXPelsPerMeter = 0;
		bmi.bmiHeader.biYPelsPerMeter = 0;
		bmi.bmiHeader.biClrUsed = 0;
		bmi.bmiHeader.biClrImportant = 0; 
		hBitmap = CreateDIBSection( NULL, (BITMAPINFO*) &bmi, DIB_RGB_COLORS, &pvBmpBits, NULL, 0 );
		hdc_offscreen = CreateCompatibleDC( NULL );
		hOldBitmap = (HBITMAP) SelectObject( hdc_offscreen, hBitmap );

		// super class 
		hInst = GetModuleHandle( NULL );
		GetClassInfo( NULL, "Button", &wc );
		wc.hInstance = hInst;
		ButtonWndProc_original = wc.lpfnWndProc;
		wc.lpfnWndProc = ButtonWndProc;
		RegisterClass( &wc );

		if (GetFileAttributes(SettingsIniPath) == INVALID_FILE_ATTRIBUTES)
		{
			FILE *fh = fopen(SettingsIniPath, "w");
			if (fh)
			{
				fputs(
					"[ddraw]\n"
					"Windowed=No\n"
					"MaintainAspectRatio=No\n"
					"AlwaysOnTop=No\n"
					"ShowWindowFrame=Yes\n"
					"IgnoreAltEnter=No\n"
					"AntiAliasedFonts=No\n"
					"SingleProcAffinity=Yes\n"
					"; 0 = Non-integer scale, 1 = Max. integer scale (if possible) or else non-integer scale, Higher value = custom integer scale\n"
					"MaximizeScale=1\n"
					"; Hack for OBS to allow 'Window Capture' with a single source - possible values = 0-1000\n"
					"OverlayFPS=5\n"
					"AdjustMouseSensitivity=Yes\n"
					"SaveSettings=Yes\n"
					"Width=640\n"
					"Height=480\n"
					"PosX=-32000\n"
					"PosY=-32000\n"
					"\n"

					, fh);

				fclose(fh);
			}
		}

		Fullscreen = !GetBool("Windowed", FALSE);
		MaintainAspectRatio = GetBool("MaintainAspectRatio", FALSE);
		AlwaysOnTop = GetBool("AlwaysOnTop", FALSE);
		ShowWindowFrame = GetBool("ShowWindowFrame", TRUE);
		IgnoreAltEnter = GetBool("IgnoreAltEnter", FALSE);
		MaximizeScale = GetInt("MaximizeScale", 1);
		OverlayFPS = GetInt("OverlayFPS", 5);
		AdjustMouseSensitivity = GetBool("AdjustMouseSensitivity", TRUE);
		SaveSettings = GetBool("SaveSettings", TRUE);

		if (!GetBool("AntiAliasedFonts", FALSE))
			HookFonts();

		if (GetBool("SingleProcAffinity", TRUE))
			SetProcessAffinityMask(GetCurrentProcess(), 1);

		Hook_PatchIAT(GetModuleHandle(NULL), "user32.dll", "CreateWindowExA", 0, (PROC)fake_CreateWindowExA);

		Hook_PatchIAT(GetModuleHandle("storm.dll"), "user32.dll", "EnableWindow", 0, (PROC)fake_EnableWindow);
		Hook_PatchIAT(GetModuleHandle("storm.dll"), "user32.dll", "CreateWindowExA", 0, (PROC)fake_CreateWindowExA);
		Hook_PatchIAT(GetModuleHandle("storm.dll"), "user32.dll", "ScreenToClient", 0, (PROC)fake_ScreenToClient);
		Hook_PatchIAT(GetModuleHandle("storm.dll"), "user32.dll", "ClientToScreen", 0, (PROC)fake_ClientToScreen);
		Hook_PatchIAT(GetModuleHandle("storm.dll"), "user32.dll", "GetWindowRect", 0, (PROC)fake_GetWindowRect);
		Hook_PatchIAT(GetModuleHandle("storm.dll"), "user32.dll", "DestroyWindow", 0, (PROC)fake_DestroyWindow);
	}

	if(ul_reason_for_call == DLL_PROCESS_DETACH )
	{
		// todo: delete dibsection...
		hOldBitmap = (HBITMAP) SelectObject( hdc_offscreen, hOldBitmap );

		if (SaveSettings)
		{
			char buf[16];

			if (!Fullscreen)
			{
				sprintf(buf, "%ld", CurrentWidth);
				WritePrivateProfileString("ddraw", "Width", buf, SettingsIniPath);

				sprintf(buf, "%ld", CurrentHeight);
				WritePrivateProfileString("ddraw", "Height", buf, SettingsIniPath);
			}

			sprintf(buf, "%ld", CurrentX);
			WritePrivateProfileString("ddraw", "PosX", buf, SettingsIniPath);

			sprintf(buf, "%ld", CurrentY);
			WritePrivateProfileString("ddraw", "PosY", buf, SettingsIniPath);

			WritePrivateProfileString("ddraw", "Windowed", !Fullscreen ? "Yes" : "No", SettingsIniPath);
			WritePrivateProfileString("ddraw", "ShowWindowFrame", ShowWindowFrame ? "Yes" : "No", SettingsIniPath);
			WritePrivateProfileString("ddraw", "AlwaysOnTop", AlwaysOnTop ? "Yes" : "No", SettingsIniPath);

			if (FullscreenFailed)
				WritePrivateProfileString("ddraw", "FullscreenFailed", "Yes", SettingsIniPath);
		}

		Hook_PatchIAT(GetModuleHandle(NULL), "user32.dll", "CreateWindowExA", 0, (PROC)CreateWindowExA);

		Hook_PatchIAT(GetModuleHandle("storm.dll"), "user32.dll", "EnableWindow", 0, (PROC)EnableWindow);
		Hook_PatchIAT(GetModuleHandle("storm.dll"), "user32.dll", "CreateWindowExA", 0, (PROC)CreateWindowExA);
		Hook_PatchIAT(GetModuleHandle("storm.dll"), "user32.dll", "ScreenToClient", 0, (PROC)ScreenToClient);
		Hook_PatchIAT(GetModuleHandle("storm.dll"), "user32.dll", "ClientToScreen", 0, (PROC)ClientToScreen);
		Hook_PatchIAT(GetModuleHandle("storm.dll"), "user32.dll", "GetWindowRect", 0, (PROC)GetWindowRect);
		Hook_PatchIAT(GetModuleHandle("storm.dll"), "user32.dll", "DestroyWindow", 0, (PROC)DestroyWindow);
	}
	return TRUE;
}

HRESULT __stdcall ddp_SetEntries( void* This, DWORD dwFlags, DWORD dwStartingEntry, DWORD dwCount, LPPALETTEENTRY lpEntries )
{
	static RGBQUAD colors[256];
	int i;
	for( i = 0; i < 256; i++ )
	{ // convert 0xFFBBGGRR to 0x00RRGGBB
		*((DWORD*)&colors[i]) = _byteswap_ulong( *(DWORD*)&lpEntries[i] ) >> 8;
	}

	SetDIBColorTable( hdc_offscreen, 0, 256, colors );

	ToScreen(FALSE); // animate palette

	// HACK // 
	// not drawing main menu after movie? 
	// this still isn't fix 100% ...
	InvalidateRect( hwnd_main, NULL, TRUE ); 
	return 0;
}

HRESULT __stdcall dd_SetDisplayMode( void* This, DWORD dwWidth, DWORD dwHeight, DWORD dwBPP )
{ 
	return 0; 
}

HRESULT __stdcall dds_Lock( void* This, LPRECT lpDestRect, LPDDSURFACEDESC lpDDSurfaceDesc, DWORD dwFlags, HANDLE hEvent )
{
	GdiFlush();
	lpDDSurfaceDesc->lPitch = OriginalWidth;
	lpDDSurfaceDesc->lpSurface = pvBmpBits;

	return 0;
}

HRESULT __stdcall dds_Unlock( void* This, LPVOID lpSurfMemPtr )
{
	ToScreen(FALSE);
	return 0;
}

HRESULT __stdcall dd_CreateSurface( void* This, LPDDSURFACEDESC lpDDSurfaceDesc, LPDIRECTDRAWSURFACE* lplpDDSurface, IUnknown* pUnkOuter )
{	
	*lplpDDSurface = (LPDIRECTDRAWSURFACE) &IDDSurf;
	GoFullscreen();
	return 0;
}

HRESULT __stdcall dd_CreatePalette( void* This, DWORD dwFlags, LPPALETTEENTRY lpColorTable, LPDIRECTDRAWPALETTE* lplpDDPalette, IUnknown* pUnkOuter)
{
	*lplpDDPalette = (LPDIRECTDRAWPALETTE) &IDDPal;
	return ddp_SetEntries( 0, 0, 0, 0, lpColorTable );
}

HRESULT __stdcall DirectDrawCreate( GUID* lpGUID, LPDIRECTDRAW* lplpDD, IUnknown* pUnkOuter )
{
	*lplpDD = (LPDIRECTDRAW) &IDDraw;
	return 0;
}

HRESULT __stdcall dd_SetCooperativeLevel( void* This, HWND hWnd, DWORD dwFlags )
{ 
	hwnd_main = hWnd;

	OrgWndProc = (WNDPROC)SetWindowLong(hWnd, GWL_WNDPROC, (LONG)WndProc);

	// the window size is the original desktop resolution...
	// which is obnoxious when not running in fullscreen.
	SetWindowPos( hWnd, HWND_TOP, 0, 0, OriginalWidth, OriginalHeight, SWP_NOOWNERZORDER | SWP_NOZORDER );

	return 0; 
}

HRESULT __stdcall dds_SetPalette( void* This, LPDIRECTDRAWPALETTE lpDDPalette )
{
	return 0;
}

HRESULT __stdcall ddp_GetEntries( void* This, DWORD dwFlags, DWORD dwBase, DWORD dwNumEntries, LPPALETTEENTRY lpEntries )
{ // can be ignored... because screen not in 8-bit mode
	return 0; 
}

HRESULT __stdcall dd_GetVerticalBlankStatus( void* This, BOOL *lpbIsInVB )
{ // ... can't get with GDI? used for movies.
	return DDERR_UNSUPPORTED;
}

HRESULT __stdcall dd_WaitForVerticalBlank( void* This, DWORD dwFlags, HANDLE hEvent)
{ 
	return DDERR_UNSUPPORTED;
}

ULONG __stdcall iunknown_Release( void* This )
{
	return 0; 
}

const DWORD dd_vtbl[] = {
	0, //QueryInterface,			   // 0x00
	0, //AddRef,					   // 0x04
	(DWORD) iunknown_Release,		   // 0x08
	0, //Compact,					   // 0x0C
	0, //CreateClipper,				   // 0x10
	(DWORD) dd_CreatePalette,		   // 0x14
	(DWORD) dd_CreateSurface,		   // 0x18
	0, //DuplicateSurface,			   // 0x1C
	0, //EnumDisplayModes,			   // 0x20
	0, //EnumSurfaces,				   // 0x24
	0, //FlipToGDISurface,			   // 0x28
	0, //GetCaps,					   // 0x2C
	0, //GetDisplayMode,			   // 0x30
	0, //GetFourCCCodes,			   // 0x34
	0, //GetGDISurface,				   // 0x38
	0, //GetMonitorFrequency,		   // 0x3C
	0, //GetScanLine,				   // 0x40
	(DWORD) dd_GetVerticalBlankStatus, // 0x44
	0, //Initialize,				   // 0x48
	0, //RestoreDisplayMode,		   // 0x4C
	(DWORD) dd_SetCooperativeLevel,	   // 0x50
	(DWORD) dd_SetDisplayMode,		   // 0x54
	(DWORD) dd_WaitForVerticalBlank,   // 0x58
};

const DWORD dds_vtbl[] = {
	0, //QueryInterface,			 // 0x00
	0, //AddRef,					 // 0x04
	(DWORD) iunknown_Release,		 // 0x08
	0, //AddAttachedSurface,		 // 0x0C
	0, //AddOverlayDirtyRect,		 // 0x10
	0, //Blt,						 // 0x14
	0, //BltBatch,					 // 0x18
	0, //BltFast,					 // 0x1C
	0, //DeleteAttachedSurface,		 // 0x20
	0, //EnumAttachedSurfaces,		 // 0x24
	0, //EnumOverlayZOrders,		 // 0x28
	0, //Flip,						 // 0x2C
	0, //GetAttachedSurface,		 // 0x30
	0, //GetBltStatus,				 // 0x34
	0, //GetCaps,					 // 0x38
	0, //GetClipper,				 // 0x3C
	0, //GetColorKey,				 // 0x40
	0, //GetDC,						 // 0x44
	0, //GetFlipStatus,				 // 0x48
	0, //GetOverlayPosition,		 // 0x4C
	0, //GetPalette,				 // 0x50
	0, //GetPixelFormat,			 // 0x54
	0, //GetSurfaceDesc,			 // 0x58
	0, //Initialize,				 // 0x5C
	0, //IsLost,					 // 0x60
	(DWORD) dds_Lock,				 // 0x64
	0, //ReleaseDC,					 // 0x68
	0, //Restore,					 // 0x6C
	0, //SetClipper,				 // 0x70
	0, //SetColorKey,				 // 0x74
	0, //SetOverlayPosition,		 // 0x78
	(DWORD) dds_SetPalette,			 // 0x7C
	(DWORD) dds_Unlock,				 // 0x80
	0, //UpdateOverlay,				 // 0x84
	0, //UpdateOverlayDisplay,		 // 0x88
	0  //UpdateOverlayZOrder,		 // 0x8C
};

const DWORD ddp_vtbl[] = { 
	0, //QueryInterface,	  // 0x00
	0, //AddRef,			  // 0x04
	(DWORD) iunknown_Release, // 0x08
	0, //GetCaps,			  // 0x0C
	(DWORD) ddp_GetEntries,	  // 0x10
	0, //Initialize,		  // 0x14
	(DWORD) ddp_SetEntries	  // 0x18
};
