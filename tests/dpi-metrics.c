/* Public XP/Wine comparison probe for DPI scaling: system metrics, fonts, icon sizes and the
 * geometry of the taskbar and Start menu windows. Output contains numbers only, never resources.
 * Usage: dpi-metrics.exe OUT [start|windows]: "start" also opens the Start menu and lists its windows,
 * "windows" only lists the visible top-level windows. */
#define _WIN32_WINNT 0x0501
#define WINVER 0x0501
#ifndef UNICODE
#define UNICODE
#endif
#include <windows.h>
#include <commctrl.h>
#include <shellapi.h>
#include <uxtheme.h>
#include <vssym32.h>
#include <stdio.h>

static FILE *output;
static HWND root;

static void font(const char *label, const LOGFONTW *f)
{
    char face[LF_FACESIZE * 3];
    WideCharToMultiByte(CP_UTF8, 0, f->lfFaceName, -1, face, sizeof(face), NULL, NULL);
    fprintf(output, "font %s face=%s height=%ld weight=%ld italic=%u charset=%u\n",
            label, face, f->lfHeight, f->lfWeight, f->lfItalic, f->lfCharSet);
}

/* The font a theme class gives a part, as GetThemeFont returns it and as the screen realizes it. */
static void theme_font(const char *label, const WCHAR *subapp, const WCHAR *classes, int part, int state, int property)
{
    static const WCHAR text[] = {0x30b9, 0x30bf, 0x30fc, 0x30c8, 0};
    HWND hwnd = CreateWindowW(L"Button", L"", WS_POPUP, 0, 0, 10, 10, NULL, NULL, NULL, NULL);
    HTHEME theme;
    LOGFONTW lf;
    HRESULT hr;
    HDC dc = GetDC(NULL);

    if (subapp) SetWindowTheme(hwnd, subapp, NULL);
    theme = OpenThemeData(hwnd, classes);
    hr = theme ? GetThemeFont(theme, dc, part, state, property, &lf) : E_FAIL;
    if (SUCCEEDED(hr))
    {
        HFONT created = CreateFontIndirectW(&lf), old = SelectObject(dc, created);
        TEXTMETRICW tm;
        SIZE size;
        GetTextMetricsW(dc, &tm);
        GetTextExtentPoint32W(dc, text, 4, &size);
        font(label, &lf);
        fprintf(output, "themefont %s tmheight=%ld ascent=%ld overhang=%ld extent=%ldx%ld\n", label, tm.tmHeight,
                tm.tmAscent, tm.tmOverhang, size.cx, size.cy);
        DeleteObject(SelectObject(dc, old));
    }
    else fprintf(output, "themefont %s hr=%08lx\n", label, (unsigned long)hr);
    if (theme) CloseThemeData(theme);
    ReleaseDC(NULL, dc);
    DestroyWindow(hwnd);
}

static BOOL CALLBACK child(HWND hwnd, LPARAM unused)
{
    WCHAR class[64], text[64];
    char class8[192], text8[192];
    RECT rect;
    HWND parent = GetAncestor(hwnd, GA_PARENT);
    int d = 0;

    (void)unused;

    if (!IsWindowVisible(hwnd)) return TRUE;
    for (; parent && parent != root && parent != GetDesktopWindow(); parent = GetAncestor(parent, GA_PARENT)) d++;
    GetClassNameW(hwnd, class, 64);
    text[0] = 0;
    GetWindowTextW(hwnd, text, 64);
    WideCharToMultiByte(CP_UTF8, 0, class, -1, class8, sizeof(class8), NULL, NULL);
    WideCharToMultiByte(CP_UTF8, 0, text, -1, text8, sizeof(text8), NULL, NULL);
    GetWindowRect(hwnd, &rect);
    fprintf(output, "win %*s%s \"%s\" %ld,%ld..%ld,%ld (%ldx%ld)\n", d * 2, "", class8, text8,
            rect.left, rect.top, rect.right, rect.bottom, rect.right - rect.left, rect.bottom - rect.top);
    if (!lstrcmpW(class, L"ToolbarWindow32") || !lstrcmpW(class, L"SysListView32"))
    {
        int count = SendMessageW(hwnd, !lstrcmpW(class, L"ToolbarWindow32") ? TB_BUTTONCOUNT : LVM_GETITEMCOUNT, 0, 0);
        HIMAGELIST list = (HIMAGELIST)SendMessageW(hwnd, !lstrcmpW(class, L"ToolbarWindow32") ? TB_GETIMAGELIST : LVM_GETIMAGELIST,
                                                   !lstrcmpW(class, L"ToolbarWindow32") ? 0 : LVSIL_SMALL, 0);
        int cx = 0, cy = 0;
        if (list) ImageList_GetIconSize(list, &cx, &cy);
        fprintf(output, "win %*s  items=%d imagelist=%dx%d\n", d * 2, "", count, cx, cy);
        if (!lstrcmpW(class, L"ToolbarWindow32"))
        {
            DWORD size = SendMessageW(hwnd, TB_GETBUTTONSIZE, 0, 0);
            fprintf(output, "win %*s  buttonsize=%dx%d\n", d * 2, "", LOWORD(size), HIWORD(size));
        }
    }
    return TRUE;
}

static BOOL CALLBACK top_level(HWND hwnd, LPARAM unused)
{
    WCHAR class[64], text[64];
    char class8[192], text8[192];
    RECT rect, client;
    POINT origin = {0, 0};

    (void)unused;
    if (!IsWindowVisible(hwnd)) return TRUE;
    GetClassNameW(hwnd, class, 64);
    text[0] = 0;
    GetWindowTextW(hwnd, text, 64);
    WideCharToMultiByte(CP_UTF8, 0, class, -1, class8, sizeof(class8), NULL, NULL);
    WideCharToMultiByte(CP_UTF8, 0, text, -1, text8, sizeof(text8), NULL, NULL);
    GetWindowRect(hwnd, &rect);
    GetClientRect(hwnd, &client);
    ClientToScreen(hwnd, &origin);
    fprintf(output, "top %s \"%s\" %ld,%ld..%ld,%ld client %ld,%ld %ldx%ld\n", class8, text8, rect.left, rect.top,
            rect.right, rect.bottom, origin.x, origin.y, client.right, client.bottom);
    return TRUE;
}

static void tree(const char *label, HWND hwnd)
{
    RECT rect;
    if (!hwnd) { fprintf(output, "tree %s missing\n", label); return; }
    GetWindowRect(hwnd, &rect);
    fprintf(output, "tree %s %ld,%ld..%ld,%ld (%ldx%ld)\n", label, rect.left, rect.top, rect.right, rect.bottom,
            rect.right - rect.left, rect.bottom - rect.top);
    root = hwnd;
    EnumChildWindows(hwnd, child, 0);
}

int main(int argc, char **argv)
{
    static const struct { int index; const char *name; } metrics[] = {
        {SM_CXSCREEN,"CXSCREEN"},{SM_CYSCREEN,"CYSCREEN"},{SM_CXVSCROLL,"CXVSCROLL"},{SM_CYHSCROLL,"CYHSCROLL"},
        {SM_CYCAPTION,"CYCAPTION"},{SM_CXBORDER,"CXBORDER"},{SM_CXDLGFRAME,"CXDLGFRAME"},{SM_CYVTHUMB,"CYVTHUMB"},
        {SM_CXICON,"CXICON"},{SM_CYICON,"CYICON"},{SM_CXCURSOR,"CXCURSOR"},{SM_CYMENU,"CYMENU"},
        {SM_CXFULLSCREEN,"CXFULLSCREEN"},{SM_CYFULLSCREEN,"CYFULLSCREEN"},{SM_CXMIN,"CXMIN"},{SM_CYMIN,"CYMIN"},
        {SM_CXSIZE,"CXSIZE"},{SM_CYSIZE,"CYSIZE"},{SM_CXFRAME,"CXFRAME"},{SM_CXMINTRACK,"CXMINTRACK"},
        {SM_CYMINTRACK,"CYMINTRACK"},{SM_CXICONSPACING,"CXICONSPACING"},{SM_CYICONSPACING,"CYICONSPACING"},
        {SM_CXEDGE,"CXEDGE"},{SM_CXMINSPACING,"CXMINSPACING"},{SM_CYMINSPACING,"CYMINSPACING"},
        {SM_CXSMICON,"CXSMICON"},{SM_CYSMICON,"CYSMICON"},{SM_CYSMCAPTION,"CYSMCAPTION"},{SM_CXSMSIZE,"CXSMSIZE"},
        {SM_CYSMSIZE,"CYSMSIZE"},{SM_CXMENUSIZE,"CXMENUSIZE"},{SM_CYMENUSIZE,"CYMENUSIZE"},{SM_CXMINIMIZED,"CXMINIMIZED"},
        {SM_CYMINIMIZED,"CYMINIMIZED"},{SM_CXMAXTRACK,"CXMAXTRACK"},{SM_CXMAXIMIZED,"CXMAXIMIZED"},
        {SM_CYMAXIMIZED,"CYMAXIMIZED"},{SM_CXMENUCHECK,"CXMENUCHECK"},{SM_CXDRAG,"CXDRAG"},{SM_CXFOCUSBORDER,"CXFOCUSBORDER"},
    };
    NONCLIENTMETRICSW ncm;
    ICONMETRICSW im;
    LOGFONTW lf;
    HIMAGELIST large = NULL, small = NULL;
    BOOL (WINAPI *get_lists)(HIMAGELIST *, HIMAGELIST *);
    HDC dc;
    RECT work;
    unsigned i;
    int cx, cy;

    output = fopen(argc > 1 ? argv[1] : "com1", "wb");
    if (!output) return 1;
    dc = GetDC(NULL);
    fprintf(output, "dpi=%d,%d\n", GetDeviceCaps(dc, LOGPIXELSX), GetDeviceCaps(dc, LOGPIXELSY));
    for (i = 0; i < 3; i++)
    {
        static const int stock[3] = { SYSTEM_FONT, DEFAULT_GUI_FONT, OEM_FIXED_FONT };
        TEXTMETRICW tm;
        SelectObject(dc, GetStockObject(stock[i]));
        GetTextMetricsW(dc, &tm);
        fprintf(output, "stockfont%u height=%ld ascent=%ld avewidth=%ld maxwidth=%ld\n", i, tm.tmHeight, tm.tmAscent,
                tm.tmAveCharWidth, tm.tmMaxCharWidth);
    }
    ReleaseDC(NULL, dc);
    for (i = 0; i < sizeof(metrics) / sizeof(metrics[0]); i++)
        fprintf(output, "sm %s=%d\n", metrics[i].name, GetSystemMetrics(metrics[i].index));
    ncm.cbSize = sizeof(ncm);
    if (SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(ncm), &ncm, 0))
    {
        fprintf(output, "ncm border=%d scroll=%dx%d caption=%dx%d smcaption=%dx%d menu=%dx%d\n", ncm.iBorderWidth,
                ncm.iScrollWidth, ncm.iScrollHeight, ncm.iCaptionWidth, ncm.iCaptionHeight, ncm.iSmCaptionWidth,
                ncm.iSmCaptionHeight, ncm.iMenuWidth, ncm.iMenuHeight);
        font("caption", &ncm.lfCaptionFont); font("smcaption", &ncm.lfSmCaptionFont);
        font("menu", &ncm.lfMenuFont); font("status", &ncm.lfStatusFont); font("message", &ncm.lfMessageFont);
    }
    im.cbSize = sizeof(im);
    if (SystemParametersInfoW(SPI_GETICONMETRICS, sizeof(im), &im, 0))
    {
        fprintf(output, "iconmetrics spacing=%dx%d wrap=%d\n", im.iHorzSpacing, im.iVertSpacing, im.iTitleWrap);
        font("icontitle", &im.lfFont);
    }
    if (SystemParametersInfoW(SPI_GETICONTITLELOGFONT, sizeof(lf), &lf, 0)) font("icontitle-spi", &lf);
    font("gui", (GetObjectW(GetStockObject(DEFAULT_GUI_FONT), sizeof(lf), &lf), &lf));
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &work, 0);
    fprintf(output, "workarea %ld,%ld..%ld,%ld\n", work.left, work.top, work.right, work.bottom);
    get_lists = (void *)GetProcAddress(LoadLibraryW(L"shell32.dll"), (LPCSTR)71);
    if (get_lists && get_lists(&large, &small))
    {
        ImageList_GetIconSize(large, &cx, &cy); fprintf(output, "shell-imagelist large=%dx%d", cx, cy);
        ImageList_GetIconSize(small, &cx, &cy); fprintf(output, " small=%dx%d\n", cx, cy);
    }
    if (IsThemeActive())
    {
        theme_font("start", L"Start", L"Button", BP_PUSHBUTTON, PBS_NORMAL, TMT_FONT);
        theme_font("clock", NULL, L"Clock", CLP_TIME, 1, TMT_FONT);
        theme_font("startpanel-user", NULL, L"StartPanel", SPP_USERPANE, 0, TMT_FONT);
        theme_font("startpanel-more", NULL, L"StartPanel", SPP_MOREPROGRAMS, 0, TMT_FONT);
        theme_font("startpanel-logoff", NULL, L"StartPanel", SPP_LOGOFFBUTTONS, 0, TMT_FONT);
        theme_font("window-caption", NULL, L"Window", WP_CAPTION, CS_ACTIVE, TMT_FONT);
        theme_font("taskband", L"TaskBand", L"Toolbar", TP_BUTTON, 1, TMT_FONT);
    }
    tree("taskbar", FindWindowW(L"Shell_TrayWnd", NULL));
    tree("desktop", FindWindowW(L"Progman", NULL));
    if (argc > 2 && !strcmp(argv[2], "windows"))
    {
        EnumWindows(top_level, 0);
        fprintf(output, "end\n");
        fclose(output);
        return 0;
    }
    if (argc > 2 && !strcmp(argv[2], "start"))
    {
        HWND start;
        keybd_event(VK_CONTROL, 0, 0, 0); keybd_event(VK_ESCAPE, 0, 0, 0);
        keybd_event(VK_ESCAPE, 0, KEYEVENTF_KEYUP, 0); keybd_event(VK_CONTROL, 0, KEYEVENTF_KEYUP, 0);
        Sleep(2500);
        start = FindWindowW(L"DV2ControlHost", NULL);
        tree("startmenu", start);
        keybd_event(VK_ESCAPE, 0, 0, 0); keybd_event(VK_ESCAPE, 0, KEYEVENTF_KEYUP, 0);
    }
    fprintf(output, "end\n");
    fclose(output);
    return 0;
}
