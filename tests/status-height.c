/* status-height: a status bar's height for several fonts, with the fonts' metrics.
 *   status-height-test OUT */
#include <windows.h>
#include <commctrl.h>
#include <stdio.h>
static void one(FILE *out, HWND parent, const WCHAR *face, int height)
{
    HWND sb = CreateWindowExW(0, STATUSCLASSNAMEW, L"x", WS_CHILD | WS_VISIBLE | SBARS_SIZEGRIP, 0, 0, 0, 0, parent, NULL, NULL, NULL);
    HFONT font = face ? CreateFontW(height, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, 0, 0, 0, 0, face) : NULL;
    HDC dc = GetDC(sb);
    TEXTMETRICW tm;
    RECT rc;
    int borders[3];
    char name[64] = "default";
    if (font) SendMessageW(sb, WM_SETFONT, (WPARAM)font, TRUE);
    SendMessageW(sb, WM_SIZE, 0, 0);
    GetWindowRect(sb, &rc);
    SelectObject(dc, font ? font : (HFONT)SendMessageW(sb, WM_GETFONT, 0, 0));
    GetTextMetricsW(dc, &tm);
    ReleaseDC(sb, dc);
    SendMessageW(sb, SB_GETBORDERS, 0, (LPARAM)borders);
    if (face) WideCharToMultiByte(CP_UTF8, 0, face, -1, name, sizeof(name), NULL, NULL);
    fprintf(out, "%s %d: height %ld tmHeight %ld ascent %ld descent %ld internal %ld external %ld borders %d,%d,%d getfont %p\n",
            name, height, rc.bottom - rc.top, tm.tmHeight, tm.tmAscent, tm.tmDescent, tm.tmInternalLeading,
            tm.tmExternalLeading, borders[0], borders[1], borders[2], (void *)SendMessageW(sb, WM_GETFONT, 0, 0));
    DestroyWindow(sb);
}
int wmain(int argc, WCHAR **argv)
{
    FILE *out = argc > 1 ? _wfopen(argv[1], L"w") : stdout;
    INITCOMMONCONTROLSEX icc = { sizeof(icc), ICC_BAR_CLASSES };
    HWND parent;
    InitCommonControlsEx(&icc);
    parent = CreateWindowW(L"STATIC", L"p", WS_OVERLAPPEDWINDOW | WS_VISIBLE, 0, 0, 400, 300, NULL, NULL, NULL, NULL);
    one(out, parent, NULL, 0);
    one(out, parent, L"Meiryo UI", -12);
    one(out, parent, L"Meiryo", -12);
    one(out, parent, L"MS UI Gothic", -12);
    one(out, parent, L"Tahoma", -11);
    one(out, parent, L"Segoe UI", -12);
    one(out, parent, L"Arial", -20);
    one(out, parent, L"Arial", -8);
    fclose(out);
    return 0;
}
