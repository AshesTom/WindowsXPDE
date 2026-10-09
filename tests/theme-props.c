/* Print the text properties of theme parts with public uxtheme calls.
 * Usage: theme-props [COM1|FILE]. Self-authored; no theme data is exported
 * beyond the property values themselves. */
#include <windows.h>
#include <uxtheme.h>
#include <vssym32.h>
#include <stdio.h>
#include <string.h>

static HANDLE out = INVALID_HANDLE_VALUE;

static void emit(const WCHAR *text)
{
    char utf8[1024];
    DWORD written;
    int length = WideCharToMultiByte(CP_UTF8, 0, text, -1, utf8, sizeof(utf8), NULL, NULL);
    if (length <= 1) return;
    if (out == INVALID_HANDLE_VALUE) fputs(utf8, stdout);
    else WriteFile(out, utf8, length - 1, &written, NULL);
}

int wmain(int argc, WCHAR **argv)
{
    static const struct { const WCHAR *cls; int part, state; } items[] = {
        {L"StartPanel", 1, 0}, {L"StartPanel", 2, 0}, {L"StartPanel", 4, 0}, {L"StartPanel", 6, 0},
        {L"StartPanel", 8, 0}, {L"StartPanel", 9, 1}, {L"Button", 1, 1}, {L"TaskBand", 0, 0},
    };
    WCHAR line[512];
    HWND hwnd;

    if (argc > 1)
    {
        out = CreateFileW(argv[1], GENERIC_WRITE, 0, NULL,
                          wcsncmp(argv[1], L"COM", 3) ? CREATE_ALWAYS : OPEN_EXISTING, 0, NULL);
        if (out != INVALID_HANDLE_VALUE && !wcsncmp(argv[1], L"COM", 3))
        {
            DCB dcb = {.DCBlength = sizeof(dcb)};
            GetCommState(out, &dcb);
            dcb.BaudRate = CBR_115200;
            dcb.ByteSize = 8;
            dcb.Parity = NOPARITY;
            dcb.StopBits = ONESTOPBIT;
            SetCommState(out, &dcb);
        }
    }
    {
        WNDCLASSW wc = {0};
        wc.lpfnWndProc = DefWindowProcW;
        wc.hInstance = GetModuleHandleW(NULL);
        wc.lpszClassName = L"ThemePropsProbe";
        RegisterClassW(&wc);
        /* XP initializes theming for a process when it opens a window's theme. */
        hwnd = CreateWindowW(wc.lpszClassName, L"", WS_POPUP, 0, 0, 1, 1, NULL, NULL, wc.hInstance, NULL);
    }
    emit(L"THEMEPROPS\r\n");
    for (unsigned i = 0; i < ARRAYSIZE(items); i++)
    {
        HTHEME theme = OpenThemeData(hwnd, items[i].cls);
        LOGFONTW font = {0};
        COLORREF color = 0, shadow = 0;
        POINT offset = {0};
        int type = -1;
        HRESULT fh, ch, th, sh, oh;
        if (!theme) continue;
        fh = GetThemeFont(theme, NULL, items[i].part, items[i].state, TMT_FONT, &font);
        ch = GetThemeColor(theme, items[i].part, items[i].state, TMT_TEXTCOLOR, &color);
        th = GetThemeEnumValue(theme, items[i].part, items[i].state, TMT_TEXTSHADOWTYPE, &type);
        sh = GetThemeColor(theme, items[i].part, items[i].state, TMT_TEXTSHADOWCOLOR, &shadow);
        oh = GetThemePosition(theme, items[i].part, items[i].state, TMT_TEXTSHADOWOFFSET, &offset);
        swprintf(line, ARRAYSIZE(line),
                 L"%ls %d %d font=%08lx '%ls' h=%ld w=%ld i=%d q=%d color=%08lx:%06lx shadow=%08lx:%d %08lx:%06lx %08lx:%ld,%ld\r\n",
                 items[i].cls, items[i].part, items[i].state, (unsigned long)fh, font.lfFaceName, font.lfHeight,
                 font.lfWeight, font.lfItalic, font.lfQuality, (unsigned long)ch, (unsigned long)color,
                 (unsigned long)th, type, (unsigned long)sh, (unsigned long)shadow, (unsigned long)oh,
                 offset.x, offset.y);
        emit(line);
        CloseThemeData(theme);
    }
    emit(L"THEMEPROPS complete\r\n");
    if (out != INVALID_HANDLE_VALUE) CloseHandle(out);
    return 0;
}
