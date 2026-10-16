/* menu-theme-layout: popup menu layout through public APIs (Windows 7's themed menus).
 *   menu-theme-layout-test OUT
 * Opens several popup menus with TrackPopupMenu at 200,150 and, while each is open,
 * writes the popup window's rectangle, GetMenuItemRect of each item and the extents of
 * each item's text (before and after the tab) in the menu font. Also the menu bar items
 * of a window with a menu. */
#include <windows.h>
#include <stdio.h>
#include <string.h>

static FILE *out;
static HMENU menus[8];
static int nmenus, current;
static HFONT menufont;
static HWND main_hwnd;

static void extent(const WCHAR *text, int len, SIZE *size)
{
    HDC dc = GetDC(NULL);
    HFONT old = SelectObject(dc, menufont);
    RECT rc = {0, 0, 0, 0};
    TEXTMETRICW tm;
    DrawTextW(dc, text, len, &rc, DT_SINGLELINE | DT_CALCRECT);
    size->cx = rc.right; size->cy = rc.bottom;
    GetTextMetricsW(dc, &tm);
    SelectObject(dc, old);
    ReleaseDC(NULL, dc);
}

static void dump(HMENU menu)
{
    HWND popup = FindWindowW(L"#32768", NULL);
    RECT rc, client;
    int i, n = GetMenuItemCount(menu);

    GetWindowRect(popup, &rc);
    GetClientRect(popup, &client);
    fprintf(out, "menu %d popup %ld,%ld,%ld,%ld client %ldx%ld style %08lx ex %08lx\n", current, rc.left, rc.top,
            rc.right, rc.bottom, client.right, client.bottom, GetWindowLongW(popup, GWL_STYLE),
            GetWindowLongW(popup, GWL_EXSTYLE));
    for (i = 0; i < n; i++)
    {
        WCHAR text[128] = {0};
        char utf[512];
        SIZE a = {0}, b = {0};
        WCHAR *tab;
        GetMenuStringW(menu, i, text, 128, MF_BYPOSITION);
        GetMenuItemRect(NULL, menu, i, &rc);
        tab = wcschr(text, '\t');
        extent(text, tab ? (int)(tab - text) : -1, &a);
        if (tab) extent(tab + 1, -1, &b);
        WideCharToMultiByte(CP_UTF8, 0, text, -1, utf, sizeof(utf), NULL, NULL);
        fprintf(out, "  item %d rect %ld,%ld,%ld,%ld text %ldx%ld accel %ldx%ld '%s'\n", i, rc.left, rc.top,
                rc.right, rc.bottom, a.cx, a.cy, b.cx, b.cy, utf);
    }
    fflush(out);
}

static LRESULT CALLBACK proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    if (msg == WM_TIMER && wp == 1)
    {
        dump(menus[current]);
        EndMenu();
        return 0;
    }
    if (msg == WM_TIMER && wp == 2)
    {
        KillTimer(hwnd, 2);
        for (current = 0; current < nmenus; current++)
        {
            SetTimer(hwnd, 1, 700, NULL);
            TrackPopupMenu(menus[current], TPM_LEFTALIGN | TPM_TOPALIGN, 200, 150, 0, hwnd, NULL);
            KillTimer(hwnd, 1);
        }
        {
            HMENU bar = GetMenu(hwnd);
            MENUBARINFO mbi;
            RECT rc;
            int i;
            memset(&mbi, 0, sizeof(mbi));
            mbi.cbSize = sizeof(mbi);
            GetMenuBarInfo(hwnd, OBJID_MENU, 0, &mbi);
            fprintf(out, "bar %ld,%ld,%ld,%ld\n", mbi.rcBar.left, mbi.rcBar.top, mbi.rcBar.right, mbi.rcBar.bottom);
            for (i = 0; i < GetMenuItemCount(bar); i++)
            {
                WCHAR text[128] = {0};
                SIZE a;
                GetMenuStringW(bar, i, text, 128, MF_BYPOSITION);
                GetMenuItemRect(hwnd, bar, i, &rc);
                extent(text, -1, &a);
                fprintf(out, "  bar item %d rect %ld,%ld,%ld,%ld text %ldx%ld\n", i, rc.left, rc.top, rc.right, rc.bottom, a.cx, a.cy);
            }
            GetWindowRect(hwnd, &rc);
            fprintf(out, "window %ld,%ld,%ld,%ld\n", rc.left, rc.top, rc.right, rc.bottom);
        }
        fclose(out);
        DestroyWindow(hwnd);
        return 0;
    }
    if (msg == WM_DESTROY) PostQuitMessage(0);
    return DefWindowProcW(hwnd, msg, wp, lp);
}

static HMENU make(const WCHAR **items)
{
    HMENU menu = CreatePopupMenu();
    for (; *items; items++)
    {
        const WCHAR *s = *items;
        if (!wcscmp(s, L"-")) AppendMenuW(menu, MF_SEPARATOR, 0, NULL);
        else if (s[0] == '>') AppendMenuW(menu, MF_POPUP, (UINT_PTR)CreatePopupMenu(), s + 1);
        else if (s[0] == '*') AppendMenuW(menu, MF_STRING | MF_CHECKED, 1, s + 1);
        else if (s[0] == '~') AppendMenuW(menu, MF_STRING | MF_GRAYED, 1, s + 1);
        else AppendMenuW(menu, MF_STRING, 1, s);
    }
    return menu;
}

int wmain(int argc, WCHAR **argv)
{
    static const WCHAR *m0[] = { L"新規(&N)\tCtrl+N", L"開く(&O)...\tCtrl+O", L"*チェック付き(&C)", L"~使えない項目(&D)",
                                 L">サブメニュー(&S)", L"-", L"終了(&X)", NULL };
    static const WCHAR *m1[] = { L"a", L"bb", NULL };
    static const WCHAR *m2[] = { L"長いテキストの項目です", L"x", NULL };
    static const WCHAR *m3[] = { L"a\tCtrl+A", L"b", NULL };
    static const WCHAR *m4[] = { L">a", L"b", NULL };
    static const WCHAR *m5[] = { L"Item\tCtrl+Shift+Alt+F12", L"-", L"-", L"Second item", NULL };
    static const WCHAR *m6[] = { L"a\tx", L">b", NULL };
    static const WCHAR *m7[] = { L"File", L"&Open", L"Wide wide wide wide", NULL };
    WNDCLASSW wc = { 0, proc, 0, 0, NULL, NULL, NULL, (HBRUSH)(COLOR_BTNFACE + 1), NULL, L"MenuProbe" };
    NONCLIENTMETRICSW ncm;
    HMENU bar = CreateMenu();
    MSG msg;

    out = argc > 1 ? _wfopen(argv[1], L"w") : stdout;
    memset(&ncm, 0, sizeof(ncm));
    ncm.cbSize = sizeof(ncm);
    SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(ncm), &ncm, 0);
    menufont = CreateFontIndirectW(&ncm.lfMenuFont);
    fprintf(out, "menu font %ld cymenu %d cymenusize %d cxmenusize %d cxmenucheck %d\n", ncm.lfMenuFont.lfHeight,
            GetSystemMetrics(SM_CYMENU), GetSystemMetrics(SM_CYMENUSIZE), GetSystemMetrics(SM_CXMENUSIZE),
            GetSystemMetrics(SM_CXMENUCHECK));
    menus[nmenus++] = make(m0); menus[nmenus++] = make(m1); menus[nmenus++] = make(m2);
    menus[nmenus++] = make(m3); menus[nmenus++] = make(m4); menus[nmenus++] = make(m5);
    menus[nmenus++] = make(m6); menus[nmenus++] = make(m7);
    AppendMenuW(bar, MF_STRING, 1, L"ファイル(&F)");
    AppendMenuW(bar, MF_STRING, 2, L"編集(&E)");
    AppendMenuW(bar, MF_STRING, 3, L"ヘルプ(&H)");
    AppendMenuW(bar, MF_STRING, 4, L"a");
    wc.hInstance = GetModuleHandleW(NULL);
    wc.hCursor = LoadCursorW(NULL, IDC_ARROW);
    RegisterClassW(&wc);
    main_hwnd = CreateWindowW(L"MenuProbe", L"menuprobe", WS_OVERLAPPEDWINDOW | WS_VISIBLE, 100, 100, 600, 400,
                              NULL, bar, NULL, NULL);
    SetTimer(main_hwnd, 2, 1000, NULL);
    while (GetMessageW(&msg, NULL, 0, 0)) { TranslateMessage(&msg); DispatchMessageW(&msg); }
    return 0;
}
