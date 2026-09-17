/*
 * Message boxes
 *
 * Copyright 1995 Bernd Schmidt
 * Copyright 2004 Ivan Leo Puoti, Juan Lang
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */

#include <stdarg.h>
#include <string.h>

#include "windef.h"
#include "winbase.h"
#include "wingdi.h"
#include "winternl.h"
#include "dlgs.h"
#include "winreg.h"
#include "user_private.h"
#include "resources.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(dialog);
WINE_DECLARE_DEBUG_CHANNEL(msgbox);

struct ThreadWindows
{
    UINT numHandles;
    UINT numAllocs;
    HWND *handles;
};

static BOOL CALLBACK MSGBOX_EnumProc(HWND hwnd, LPARAM lParam)
{
    struct ThreadWindows *threadWindows = (struct ThreadWindows *)lParam;

    if (!NtUserEnableWindow(hwnd, FALSE))
    {
        if(threadWindows->numHandles >= threadWindows->numAllocs)
        {
            threadWindows->handles = HeapReAlloc(GetProcessHeap(), 0, threadWindows->handles,
                                                 (threadWindows->numAllocs*2)*sizeof(HWND));
            threadWindows->numAllocs *= 2;
        }
        threadWindows->handles[threadWindows->numHandles++]=hwnd;
    }
   return TRUE;
}

/* Layout metrics for the modern message box. The classic one derives every
 * size from the text, which is why it ends up cramped next to a dialog from
 * any current desktop. */
#define MSGBOX_MARGIN            20
#define MSGBOX_ICON_GAP          16
#define MSGBOX_CONTENT_GAP       16
#define MSGBOX_BUTTON_GAP         6
#define MSGBOX_BUTTON_HEIGHT     32
#define MSGBOX_BUTTON_MIN_WIDTH  80
#define MSGBOX_BUTTON_PADDING    16
#define MSGBOX_BUTTON_RADIUS      3

/* The rounded corners are drawn at this multiple and averaged back down,
 * because RoundRect has no antialiasing and leaves visible steps. */
#define MSGBOX_SUPERSAMPLE        4

/* @@ Wine registry key: HKCU\Software\TuxBlox\MessageBox */
static BOOL MSGBOX_ClassicStyle(void)
{
    WCHAR value[8];
    DWORD size = sizeof(value), type;
    BOOL classic = FALSE;
    HKEY hkey;

    if (!RegOpenKeyExW( HKEY_CURRENT_USER, L"Software\\TuxBlox\\MessageBox", 0, KEY_READ, &hkey ))
    {
        if (!RegQueryValueExW( hkey, L"Classic", NULL, &type, (BYTE *)value, &size ) && type == REG_SZ)
            classic = (value[0] == '1' || value[0] == 'y' || value[0] == 'Y');
        RegCloseKey( hkey );
    }
    return classic;
}

static BOOL MSGBOX_IsDarkColor( COLORREF color )
{
    return (GetRValue(color) * 30 + GetGValue(color) * 59 + GetBValue(color) * 11) / 100 < 128;
}

/* Positive lightens towards white, negative darkens towards black. */
static COLORREF MSGBOX_Shade( COLORREF color, int percent )
{
    int r = GetRValue(color), g = GetGValue(color), b = GetBValue(color);

    if (percent > 0)
    {
        r += (255 - r) * percent / 100;
        g += (255 - g) * percent / 100;
        b += (255 - b) * percent / 100;
    }
    else
    {
        r += r * percent / 100;
        g += g * percent / 100;
        b += b * percent / 100;
    }
    return RGB( r, g, b );
}

static void MSGBOX_DrawButton( HWND hwnd, const DRAWITEMSTRUCT *dis )
{
    int width = dis->rcItem.right - dis->rcItem.left;
    int height = dis->rcItem.bottom - dis->rcItem.top;
    int scale = MSGBOX_SUPERSAMPLE, samples = scale * scale;
    COLORREF back = GetSysColor( COLOR_3DFACE );
    BOOL dark = MSGBOX_IsDarkColor( back );
    COLORREF face, edge;
    BITMAPINFO bmi;
    DWORD *bits, *flat;
    HBITMAP bmp, flatbmp, oldbmp, oldflat;
    HDC mem, out;
    HBRUSH brush, oldbrush;
    HPEN pen, oldpen;
    WCHAR text[256];
    RECT rc;
    int x, y, sx, sy;

    if (width <= 0 || height <= 0) return;

    if (dis->itemState & ODS_SELECTED) face = MSGBOX_Shade( back, dark ? -8 : -10 );
    else face = MSGBOX_Shade( back, dark ? 12 : 22 );

    /* A focused or default button is outlined in the accent colour, the way
     * every current desktop marks the one Enter will press. */
    if (dis->itemState & (ODS_FOCUS | ODS_DEFAULT) ||
        LOWORD(SendMessageW( hwnd, DM_GETDEFID, 0, 0 )) == dis->CtlID)
        edge = GetSysColor( COLOR_HIGHLIGHT );
    else
        edge = MSGBOX_Shade( back, dark ? 34 : -22 );

    memset( &bmi, 0, sizeof(bmi) );
    bmi.bmiHeader.biSize = sizeof(bmi.bmiHeader);
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;
    bmi.bmiHeader.biWidth = width * scale;
    bmi.bmiHeader.biHeight = -height * scale;

    mem = CreateCompatibleDC( dis->hDC );
    bmp = CreateDIBSection( dis->hDC, &bmi, DIB_RGB_COLORS, (void **)&bits, NULL, 0 );
    bmi.bmiHeader.biWidth = width;
    bmi.bmiHeader.biHeight = -height;
    out = CreateCompatibleDC( dis->hDC );
    flatbmp = CreateDIBSection( dis->hDC, &bmi, DIB_RGB_COLORS, (void **)&flat, NULL, 0 );
    if (!mem || !out || !bmp || !flatbmp)
    {
        if (bmp) DeleteObject( bmp );
        if (flatbmp) DeleteObject( flatbmp );
        if (mem) DeleteDC( mem );
        if (out) DeleteDC( out );
        return;
    }
    oldbmp = SelectObject( mem, bmp );
    oldflat = SelectObject( out, flatbmp );

    rc.left = rc.top = 0;
    rc.right = width * scale;
    rc.bottom = height * scale;
    brush = CreateSolidBrush( back );
    FillRect( mem, &rc, brush );
    DeleteObject( brush );

    brush = CreateSolidBrush( face );
    pen = CreatePen( PS_SOLID, scale, edge );
    oldbrush = SelectObject( mem, brush );
    oldpen = SelectObject( mem, pen );
    RoundRect( mem, scale / 2, scale / 2, width * scale - scale / 2, height * scale - scale / 2,
               MSGBOX_BUTTON_RADIUS * 2 * scale, MSGBOX_BUTTON_RADIUS * 2 * scale );
    SelectObject( mem, oldbrush );
    SelectObject( mem, oldpen );
    DeleteObject( brush );
    DeleteObject( pen );

    for (y = 0; y < height; y++)
    {
        for (x = 0; x < width; x++)
        {
            unsigned int r = 0, g = 0, b = 0;

            for (sy = 0; sy < scale; sy++)
            {
                const DWORD *row = bits + (y * scale + sy) * width * scale + x * scale;
                for (sx = 0; sx < scale; sx++)
                {
                    r += (row[sx] >> 16) & 0xff;
                    g += (row[sx] >> 8) & 0xff;
                    b += row[sx] & 0xff;
                }
            }
            flat[y * width + x] = ((r / samples) << 16) | ((g / samples) << 8) | (b / samples);
        }
    }
    BitBlt( dis->hDC, dis->rcItem.left, dis->rcItem.top, width, height, out, 0, 0, SRCCOPY );

    SelectObject( mem, oldbmp );
    SelectObject( out, oldflat );
    DeleteObject( bmp );
    DeleteObject( flatbmp );
    DeleteDC( mem );
    DeleteDC( out );

    if (GetWindowTextW( dis->hwndItem, text, ARRAY_SIZE(text) ))
    {
        SetBkMode( dis->hDC, TRANSPARENT );
        SetTextColor( dis->hDC, GetSysColor( (dis->itemState & ODS_DISABLED) ? COLOR_GRAYTEXT
                                                                            : COLOR_BTNTEXT ));
        rc = dis->rcItem;
        DrawTextW( dis->hDC, text, -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE );
    }
}

static void MSGBOX_OnInit(HWND hwnd, LPMSGBOXPARAMSW lpmb)
{
    HFONT hPrevFont;
    RECT rect;
    HWND hItem;
    HDC hdc;
    int i, buttons;
    int bspace, bw, bh, theight, tleft, wwidth, wheight, wleft, wtop, bpos;
    int btop, itop, ttop, contentheight, clientwidth;
    BOOL classic = MSGBOX_ClassicStyle();
    int borheight, borwidth, iheight, ileft, iwidth, twidth, tiheight;
    NONCLIENTMETRICSW nclm;
    HMONITOR monitor;
    MONITORINFO mon_info;
    LPCWSTR lpszText;
    WCHAR *buffer = NULL;
    const WCHAR *ptr;

    /* Index the order the buttons need to appear to an ID* constant */
    static const int buttonOrder[10] = { IDYES, IDNO, IDOK, IDABORT, IDRETRY,
                                         IDCANCEL, IDIGNORE, IDTRYAGAIN,
                                         IDCONTINUE, IDHELP };

    nclm.cbSize = sizeof(nclm);
    SystemParametersInfoW (SPI_GETNONCLIENTMETRICS, 0, &nclm, 0);

    if (!IS_INTRESOURCE(lpmb->lpszCaption)) {
       SetWindowTextW(hwnd, lpmb->lpszCaption);
    } else {
        UINT len = LoadStringW( lpmb->hInstance, LOWORD(lpmb->lpszCaption), (LPWSTR)&ptr, 0 );
        if (!len) len = LoadStringW( user32_module, IDS_ERROR, (LPWSTR)&ptr, 0 );
        buffer = HeapAlloc( GetProcessHeap(), 0, (len + 1) * sizeof(WCHAR) );
        if (buffer)
        {
            memcpy( buffer, ptr, len * sizeof(WCHAR) );
            buffer[len] = 0;
            SetWindowTextW( hwnd, buffer );
            HeapFree( GetProcessHeap(), 0, buffer );
            buffer = NULL;
        }
    }
    if (IS_INTRESOURCE(lpmb->lpszText)) {
        UINT len = LoadStringW( lpmb->hInstance, LOWORD(lpmb->lpszText), (LPWSTR)&ptr, 0 );
        lpszText = buffer = HeapAlloc( GetProcessHeap(), 0, (len + 1) * sizeof(WCHAR) );
        if (buffer)
        {
            memcpy( buffer, ptr, len * sizeof(WCHAR) );
            buffer[len] = 0;
        }
    } else {
       lpszText = lpmb->lpszText;
    }

    /* handle modal message boxes */
    if (lpmb->dwStyle & MB_TASKMODAL && lpmb->hwndOwner == NULL)
        NtUserSetWindowPos( hwnd, lpmb->dwStyle & MB_TOPMOST ? HWND_TOPMOST : HWND_TOP, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE );
    else if (lpmb->dwStyle & MB_SYSTEMMODAL)
        NtUserSetWindowPos( hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_FRAMECHANGED );
    else if (lpmb->dwStyle & MB_TOPMOST)
        NtUserSetWindowPos( hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE );

    TRACE_(msgbox)("%s\n", debugstr_w(lpszText));
    SetWindowTextW(GetDlgItem(hwnd, MSGBOX_IDTEXT), lpszText);

    /* Remove not selected buttons and assign the WS_GROUP style to the first button */
    hItem = 0;
    switch(lpmb->dwStyle & MB_TYPEMASK) {
    case MB_OK:
	NtUserDestroyWindow(GetDlgItem(hwnd, IDCANCEL));
	/* fall through */
    case MB_OKCANCEL:
	hItem = GetDlgItem(hwnd, IDOK);
	NtUserDestroyWindow(GetDlgItem(hwnd, IDABORT));
	NtUserDestroyWindow(GetDlgItem(hwnd, IDRETRY));
	NtUserDestroyWindow(GetDlgItem(hwnd, IDIGNORE));
	NtUserDestroyWindow(GetDlgItem(hwnd, IDYES));
	NtUserDestroyWindow(GetDlgItem(hwnd, IDNO));
	NtUserDestroyWindow(GetDlgItem(hwnd, IDTRYAGAIN));
	NtUserDestroyWindow(GetDlgItem(hwnd, IDCONTINUE));
	break;
    case MB_ABORTRETRYIGNORE:
	hItem = GetDlgItem(hwnd, IDABORT);
	NtUserDestroyWindow(GetDlgItem(hwnd, IDOK));
	NtUserDestroyWindow(GetDlgItem(hwnd, IDCANCEL));
	NtUserDestroyWindow(GetDlgItem(hwnd, IDYES));
	NtUserDestroyWindow(GetDlgItem(hwnd, IDNO));
	NtUserDestroyWindow(GetDlgItem(hwnd, IDCONTINUE));
	NtUserDestroyWindow(GetDlgItem(hwnd, IDTRYAGAIN));
	break;
    case MB_YESNO:
	NtUserDestroyWindow(GetDlgItem(hwnd, IDCANCEL));
	/* fall through */
    case MB_YESNOCANCEL:
	hItem = GetDlgItem(hwnd, IDYES);
	NtUserDestroyWindow(GetDlgItem(hwnd, IDOK));
	NtUserDestroyWindow(GetDlgItem(hwnd, IDABORT));
	NtUserDestroyWindow(GetDlgItem(hwnd, IDRETRY));
	NtUserDestroyWindow(GetDlgItem(hwnd, IDIGNORE));
	NtUserDestroyWindow(GetDlgItem(hwnd, IDCONTINUE));
	NtUserDestroyWindow(GetDlgItem(hwnd, IDTRYAGAIN));
	break;
    case MB_RETRYCANCEL:
	hItem = GetDlgItem(hwnd, IDRETRY);
	NtUserDestroyWindow(GetDlgItem(hwnd, IDOK));
	NtUserDestroyWindow(GetDlgItem(hwnd, IDABORT));
	NtUserDestroyWindow(GetDlgItem(hwnd, IDIGNORE));
	NtUserDestroyWindow(GetDlgItem(hwnd, IDYES));
	NtUserDestroyWindow(GetDlgItem(hwnd, IDNO));
	NtUserDestroyWindow(GetDlgItem(hwnd, IDCONTINUE));
	NtUserDestroyWindow(GetDlgItem(hwnd, IDTRYAGAIN));
	break;
    case MB_CANCELTRYCONTINUE:
	hItem = GetDlgItem(hwnd, IDCANCEL);
	NtUserDestroyWindow(GetDlgItem(hwnd, IDOK));
	NtUserDestroyWindow(GetDlgItem(hwnd, IDABORT));
	NtUserDestroyWindow(GetDlgItem(hwnd, IDIGNORE));
	NtUserDestroyWindow(GetDlgItem(hwnd, IDYES));
	NtUserDestroyWindow(GetDlgItem(hwnd, IDNO));
	NtUserDestroyWindow(GetDlgItem(hwnd, IDRETRY));
    }

    if (hItem) SetWindowLongW(hItem, GWL_STYLE, GetWindowLongW(hItem, GWL_STYLE) | WS_GROUP);

    /* Set the icon */
    switch(lpmb->dwStyle & MB_ICONMASK) {
    case MB_ICONEXCLAMATION:
	SendDlgItemMessageW(hwnd, MSGBOX_IDICON, STM_SETICON,
			    (WPARAM)LoadIconW(0, (LPWSTR)IDI_EXCLAMATION), 0);
	break;
    case MB_ICONQUESTION:
	SendDlgItemMessageW(hwnd, MSGBOX_IDICON, STM_SETICON,
			    (WPARAM)LoadIconW(0, (LPWSTR)IDI_QUESTION), 0);
	break;
    case MB_ICONASTERISK:
	SendDlgItemMessageW(hwnd, MSGBOX_IDICON, STM_SETICON,
			    (WPARAM)LoadIconW(0, (LPWSTR)IDI_ASTERISK), 0);
	break;
    case MB_ICONHAND:
      SendDlgItemMessageW(hwnd, MSGBOX_IDICON, STM_SETICON,
			    (WPARAM)LoadIconW(0, (LPWSTR)IDI_HAND), 0);
      break;
    case MB_USERICON:
      SendDlgItemMessageW(hwnd, MSGBOX_IDICON, STM_SETICON,
			  (WPARAM)LoadIconW(lpmb->hInstance, lpmb->lpszIcon), 0);
      break;
    default:
	/* By default, Windows 95/98/NT do not associate an icon to message boxes.
         * So wine should do the same.
         */
	break;
    }

    /* Remove Help button unless MB_HELP supplied */
    if (!(lpmb->dwStyle & MB_HELP)) {
        NtUserDestroyWindow(GetDlgItem(hwnd, IDHELP));
    }

    /* Position everything */
    GetWindowRect(hwnd, &rect);
    borheight = rect.bottom - rect.top;
    borwidth  = rect.right - rect.left;
    GetClientRect(hwnd, &rect);
    borheight -= rect.bottom - rect.top;
    borwidth  -= rect.right - rect.left;

    /* Get the icon height */
    GetWindowRect(GetDlgItem(hwnd, MSGBOX_IDICON), &rect);
    MapWindowPoints(0, hwnd, (LPPOINT)&rect, 2);
    if (!(lpmb->dwStyle & MB_ICONMASK))
    {
        rect.bottom = rect.top;
        rect.right = rect.left;
    }
    iheight = rect.bottom - rect.top;
    ileft = rect.left;
    iwidth = rect.right - ileft;

    hdc = NtUserGetDC(hwnd);
    hPrevFont = SelectObject( hdc, (HFONT)SendMessageW( hwnd, WM_GETFONT, 0, 0 ));

    /* Get the number of visible buttons and their size */
    bh = bw = 1; /* Minimum button sizes */
    for (buttons = 0, i = IDOK; i <= IDCONTINUE; i++)
    {
        if (i == IDCLOSE) continue; /* No CLOSE button */
	hItem = GetDlgItem(hwnd, i);
	if (GetWindowLongW(hItem, GWL_STYLE) & WS_VISIBLE)
	{
	    WCHAR buttonText[1024];
	    int w, h;
	    buttons++;
	    if (GetWindowTextW(hItem, buttonText, 1024))
	    {
		DrawTextW( hdc, buttonText, -1, &rect, DT_LEFT | DT_EXPANDTABS | DT_CALCRECT);
		h = rect.bottom - rect.top;
		w = rect.right - rect.left;
		if (h > bh) bh = h;
		if (w > bw)  bw = w ;
	    }
	}
    }
    if (classic)
    {
        bw = max(bw, bh * 2);
        /* Button white space */
        bh = bh * 2;
        bw = bw * 2;
        bspace = bw/3; /* Space between buttons */
    }
    else
    {
        bw = max(MSGBOX_BUTTON_MIN_WIDTH, bw + MSGBOX_BUTTON_PADDING * 2);
        bh = MSGBOX_BUTTON_HEIGHT;
        bspace = MSGBOX_BUTTON_GAP;
    }

    /* Get the text size */
    GetClientRect(GetDlgItem(hwnd, MSGBOX_IDTEXT), &rect);
    rect.top = rect.left = rect.bottom = 0;
    DrawTextW(hdc, lpszText, -1, &rect,
              DT_LEFT | DT_EXPANDTABS | DT_WORDBREAK | DT_CALCRECT | DT_NOPREFIX);
    if (classic)
    {
        /* Min text width corresponds to space for the buttons */
        tleft = ileft;
        if (iwidth) tleft += ileft + iwidth;
        twidth = max((bw + bspace) * buttons + bspace - tleft, rect.right);
    }
    else
    {
        ileft = MSGBOX_MARGIN;
        tleft = MSGBOX_MARGIN + (iwidth ? iwidth + MSGBOX_ICON_GAP : 0);
        twidth = rect.right;
    }
    theight = rect.bottom;

    SelectObject(hdc, hPrevFont);
    NtUserReleaseDC( hwnd, hdc );

    contentheight = max(iheight, theight);
    if (classic)
    {
        tiheight = 16 + contentheight;
        wwidth  = tleft + twidth + ileft + borwidth;
        wheight = 8 + tiheight + bh + borheight;
        itop = (tiheight - iheight) / 2;
        ttop = (tiheight - theight) / 2;
        btop = tiheight;
    }
    else
    {
        /* The button row is never narrower than the buttons it holds, so a
         * short message still gets a dialog wide enough for them. */
        clientwidth = max(tleft + twidth + MSGBOX_MARGIN,
                          MSGBOX_MARGIN * 2 + (bw + bspace) * buttons - bspace);
        btop = MSGBOX_MARGIN + contentheight + MSGBOX_CONTENT_GAP;
        wwidth = clientwidth + borwidth;
        wheight = btop + bh + MSGBOX_MARGIN + borheight;
        itop = MSGBOX_MARGIN + (contentheight - iheight) / 2;
        ttop = MSGBOX_MARGIN + (contentheight - theight) / 2;
    }

    /* Message boxes are always desktop centered, so query desktop size and center window */
    monitor = MonitorFromWindow(lpmb->hwndOwner ? lpmb->hwndOwner : GetActiveWindow(), MONITOR_DEFAULTTOPRIMARY);
    mon_info.cbSize = sizeof(mon_info);
    GetMonitorInfoW(monitor, &mon_info);
    wleft = (mon_info.rcWork.left + mon_info.rcWork.right - wwidth) / 2;
    wtop = (mon_info.rcWork.top + mon_info.rcWork.bottom - wheight) / 2;

    /* Resize and center the window */
    NtUserSetWindowPos( hwnd, 0, wleft, wtop, wwidth, wheight,
                        SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOREDRAW );

    /* Position the icon */
    NtUserSetWindowPos( GetDlgItem(hwnd, MSGBOX_IDICON), 0, ileft, itop, 0, 0,
                        SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOREDRAW );

    /* Position the text */
    NtUserSetWindowPos( GetDlgItem(hwnd, MSGBOX_IDTEXT), 0, tleft, ttop, twidth, theight,
                        SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOREDRAW );

    /* Position the buttons */
    if (classic)
        bpos = (wwidth - (bw + bspace) * buttons + bspace) / 2;
    else
        bpos = clientwidth - MSGBOX_MARGIN - ((bw + bspace) * buttons - bspace);
    for (buttons = i = 0; i < ARRAY_SIZE(buttonOrder); i++) {

	/* Convert the button order to ID* value to order for the buttons */
	hItem = GetDlgItem(hwnd, buttonOrder[i]);
	if (GetWindowLongW(hItem, GWL_STYLE) & WS_VISIBLE) {
	    if (buttons++ == ((lpmb->dwStyle & MB_DEFMASK) >> 8)) {
		NtUserSetFocus(hItem);
		if (classic)
		    SendMessageW( hItem, BM_SETSTYLE, BS_DEFPUSHBUTTON, TRUE );
		else
		    SendMessageW( hwnd, DM_SETDEFID, buttonOrder[i], 0 );
	    }
	    if (!classic)
		SetWindowLongW( hItem, GWL_STYLE, GetWindowLongW( hItem, GWL_STYLE ) | BS_OWNERDRAW );
	    NtUserSetWindowPos( hItem, 0, bpos, btop, bw, bh,
                                SWP_NOZORDER|SWP_NOACTIVATE|SWP_NOREDRAW );
	    bpos += bw + bspace;
	}
    }

    HeapFree( GetProcessHeap(), 0, buffer );
}


/**************************************************************************
 *           MSGBOX_DlgProc
 *
 * Dialog procedure for message boxes.
 */
static INT_PTR CALLBACK MSGBOX_DlgProc( HWND hwnd, UINT message,
                                        WPARAM wParam, LPARAM lParam )
{
  switch(message) {
   case WM_INITDIALOG:
   {
       LPMSGBOXPARAMSW mbp = (LPMSGBOXPARAMSW)lParam;
       NtUserSetWindowContextHelpId(hwnd, mbp->dwContextHelpId);
       MSGBOX_OnInit(hwnd, mbp);
       SetPropA(hwnd, "WINE_MSGBOX_HELPCALLBACK", mbp->lpfnMsgBoxCallback);
       break;
   }

   case WM_DRAWITEM:
    MSGBOX_DrawButton( hwnd, (const DRAWITEMSTRUCT *)lParam );
    return TRUE;

   case WM_COMMAND:
    switch (LOWORD(wParam))
    {
     case IDOK:
     case IDCANCEL:
     case IDABORT:
     case IDRETRY:
     case IDIGNORE:
     case IDYES:
     case IDNO:
     case IDTRYAGAIN:
     case IDCONTINUE:
      EndDialog(hwnd, wParam);
      break;
     case IDHELP:
      FIXME("Help button not supported yet\n");
      break;
    }
    break;

    case WM_HELP:
    {
        MSGBOXCALLBACK callback = (MSGBOXCALLBACK)GetPropA(hwnd, "WINE_MSGBOX_HELPCALLBACK");
        HELPINFO hi;

        memcpy(&hi, (void *)lParam, sizeof(hi));
        hi.dwContextId = NtUserGetWindowContextHelpId(hwnd);

        if (callback)
            callback(&hi);
        else
            SendMessageW(GetWindow(hwnd, GW_OWNER), WM_HELP, 0, (LPARAM)&hi);
	break;
   }

   default:
     /* Ok. Ignore all the other messages */
     TRACE("Message number 0x%04x is being ignored.\n", message);
    break;
  }
  return 0;
}


/**************************************************************************
 *		MessageBoxA (USER32.@)
 */
INT WINAPI MessageBoxA(HWND hWnd, LPCSTR text, LPCSTR title, UINT type)
{
    return MessageBoxExA(hWnd, text, title, type, LANG_NEUTRAL);
}


/**************************************************************************
 *		MessageBoxW (USER32.@)
 */
INT WINAPI MessageBoxW( HWND hwnd, LPCWSTR text, LPCWSTR title, UINT type )
{
    return MessageBoxExW(hwnd, text, title, type, LANG_NEUTRAL);
}


/**************************************************************************
 *		MessageBoxExA (USER32.@)
 */
INT WINAPI MessageBoxExA( HWND hWnd, LPCSTR text, LPCSTR title,
                              UINT type, WORD langid )
{
    MSGBOXPARAMSA msgbox;

    msgbox.cbSize = sizeof(msgbox);
    msgbox.hwndOwner = hWnd;
    msgbox.hInstance = 0;
    msgbox.lpszText = text;
    msgbox.lpszCaption = title;
    msgbox.dwStyle = type;
    msgbox.lpszIcon = NULL;
    msgbox.dwContextHelpId = 0;
    msgbox.lpfnMsgBoxCallback = NULL;
    msgbox.dwLanguageId = langid;

    return MessageBoxIndirectA(&msgbox);
}

/**************************************************************************
 *		MessageBoxExW (USER32.@)
 */
INT WINAPI MessageBoxExW( HWND hWnd, LPCWSTR text, LPCWSTR title,
                              UINT type, WORD langid )
{
    MSGBOXPARAMSW msgbox;

    msgbox.cbSize = sizeof(msgbox);
    msgbox.hwndOwner = hWnd;
    msgbox.hInstance = 0;
    msgbox.lpszText = text;
    msgbox.lpszCaption = title;
    msgbox.dwStyle = type;
    msgbox.lpszIcon = NULL;
    msgbox.dwContextHelpId = 0;
    msgbox.lpfnMsgBoxCallback = NULL;
    msgbox.dwLanguageId = langid;

    return MessageBoxIndirectW(&msgbox);
}

/**************************************************************************
 *      MessageBoxTimeoutA (USER32.@)
 */
INT WINAPI MessageBoxTimeoutA( HWND hWnd, LPCSTR text, LPCSTR title,
                               UINT type, WORD langid, DWORD timeout )
{
    FIXME("timeout not supported (%lu)\n", timeout);
    return MessageBoxExA( hWnd, text, title, type, langid );
}

/**************************************************************************
 *      MessageBoxTimeoutW (USER32.@)
 */
INT WINAPI MessageBoxTimeoutW( HWND hWnd, LPCWSTR text, LPCWSTR title,
                               UINT type, WORD langid, DWORD timeout )
{
    FIXME("timeout not supported (%lu)\n", timeout);
    return MessageBoxExW( hWnd, text, title, type, langid );
}

/**************************************************************************
 *		MessageBoxIndirectA (USER32.@)
 */
INT WINAPI MessageBoxIndirectA( LPMSGBOXPARAMSA msgbox )
{
    MSGBOXPARAMSW msgboxW;
    UNICODE_STRING textW, captionW, iconW;
    int ret;

    if (IS_INTRESOURCE(msgbox->lpszText))
        textW.Buffer = (LPWSTR)msgbox->lpszText;
    else
        RtlCreateUnicodeStringFromAsciiz(&textW, msgbox->lpszText);
    if (IS_INTRESOURCE(msgbox->lpszCaption))
        captionW.Buffer = (LPWSTR)msgbox->lpszCaption;
    else
        RtlCreateUnicodeStringFromAsciiz(&captionW, msgbox->lpszCaption);

    if (msgbox->dwStyle & MB_USERICON)
    {
        if (IS_INTRESOURCE(msgbox->lpszIcon))
            iconW.Buffer = (LPWSTR)msgbox->lpszIcon;
        else
            RtlCreateUnicodeStringFromAsciiz(&iconW, msgbox->lpszIcon);
    }
    else
        iconW.Buffer = NULL;

    msgboxW.cbSize = sizeof(msgboxW);
    msgboxW.hwndOwner = msgbox->hwndOwner;
    msgboxW.hInstance = msgbox->hInstance;
    msgboxW.lpszText = textW.Buffer;
    msgboxW.lpszCaption = captionW.Buffer;
    msgboxW.dwStyle = msgbox->dwStyle;
    msgboxW.lpszIcon = iconW.Buffer;
    msgboxW.dwContextHelpId = msgbox->dwContextHelpId;
    msgboxW.lpfnMsgBoxCallback = msgbox->lpfnMsgBoxCallback;
    msgboxW.dwLanguageId = msgbox->dwLanguageId;

    ret = MessageBoxIndirectW(&msgboxW);

    if (!IS_INTRESOURCE(textW.Buffer)) RtlFreeUnicodeString(&textW);
    if (!IS_INTRESOURCE(captionW.Buffer)) RtlFreeUnicodeString(&captionW);
    if (!IS_INTRESOURCE(iconW.Buffer)) RtlFreeUnicodeString(&iconW);
    return ret;
}

/**************************************************************************
 *		MessageBoxIndirectW (USER32.@)
 */
INT WINAPI MessageBoxIndirectW( LPMSGBOXPARAMSW msgbox )
{
    LPVOID tmplate;
    HRSRC hRes;
    int ret;
    UINT i;
    struct ThreadWindows threadWindows;

    if (!(hRes = FindResourceExW(user32_module, (LPWSTR)RT_DIALOG, L"MSGBOX", msgbox->dwLanguageId)))
    {
        if (!msgbox->dwLanguageId ||
            !(hRes = FindResourceExW(user32_module, (LPWSTR)RT_DIALOG, L"MSGBOX", LANG_NEUTRAL)))
            return 0;
    }
    if (!(tmplate = LoadResource(user32_module, hRes)))
        return 0;

    if ((msgbox->dwStyle & MB_TASKMODAL) && (msgbox->hwndOwner==NULL))
    {
        threadWindows.numHandles = 0;
        threadWindows.numAllocs = 10;
        threadWindows.handles = HeapAlloc(GetProcessHeap(), 0, 10*sizeof(HWND));
        EnumThreadWindows(GetCurrentThreadId(), MSGBOX_EnumProc, (LPARAM)&threadWindows);
    }

    NtUserModifyUserStartupInfoFlags( STARTF_USESHOWWINDOW, 0 );
    ret=DialogBoxIndirectParamW(msgbox->hInstance, tmplate,
                                msgbox->hwndOwner, MSGBOX_DlgProc, (LPARAM)msgbox);

    if ((msgbox->dwStyle & MB_TASKMODAL) && (msgbox->hwndOwner==NULL))
    {
        for (i = 0; i < threadWindows.numHandles; i++)
            NtUserEnableWindow(threadWindows.handles[i], TRUE);
        HeapFree(GetProcessHeap(), 0, threadWindows.handles);
    }
    return ret;
}
