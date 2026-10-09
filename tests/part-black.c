/* Draw a few theme parts with alpha onto black and print the pixels, which
 * are then the theme manager's premultiplied colours. Usage: part-black [COM1]
 * Public uxtheme calls only. */
#include <windows.h>
#include <uxtheme.h>
#include <stdio.h>
#include <string.h>

static HANDLE out = INVALID_HANDLE_VALUE;

static void emit(const char *text)
{
    DWORD written;
    if (out == INVALID_HANDLE_VALUE) fputs(text, stdout);
    else WriteFile(out, text, (DWORD)strlen(text), &written, NULL);
}

int main(int argc, char **argv)
{
    static const struct { const WCHAR *cls; int part, state, width, height; } parts[] = {
        {L"Button", 2, 5, 13, 13}, {L"Button", 3, 5, 13, 13}, {L"Button", 1, 1, 24, 23}, {L"Window", 18, 1, 21, 21},
    };
    HWND hwnd;
    char line[4096];

    if (argc > 1)
    {
        out = CreateFileA(argv[1], GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
        if (out != INVALID_HANDLE_VALUE)
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
    hwnd = CreateWindowA("STATIC", "", WS_POPUP, 0, 0, 1, 1, NULL, NULL, NULL, NULL);
    emit("PARTBLACK\r\n");
    for (unsigned i = 0; i < sizeof(parts) / sizeof(parts[0]); i++)
    {
        HTHEME theme = OpenThemeData(hwnd, parts[i].cls);
        BITMAPINFO info = {0};
        DWORD *bits;
        HDC dc = CreateCompatibleDC(NULL);
        RECT rect = {0, 0, parts[i].width, parts[i].height};
        if (!theme) continue;
        info.bmiHeader.biSize = sizeof(info.bmiHeader);
        info.bmiHeader.biWidth = parts[i].width;
        info.bmiHeader.biHeight = -parts[i].height;
        info.bmiHeader.biPlanes = 1;
        info.bmiHeader.biBitCount = 32;
        SelectObject(dc, CreateDIBSection(dc, &info, DIB_RGB_COLORS, (void **)&bits, NULL, 0));
        memset(bits, 0, parts[i].width * parts[i].height * 4);
        DrawThemeBackground(theme, dc, parts[i].part, parts[i].state, &rect, NULL);
        GdiFlush();
        for (int y = 0; y < parts[i].height; y++)
        {
            int n = sprintf(line, "p%u y%d:", i, y);
            for (int x = 0; x < parts[i].width; x++) n += sprintf(line + n, " %06lx", bits[y * parts[i].width + x] & 0xffffff);
            sprintf(line + n, "\r\n");
            emit(line);
        }
        CloseThemeData(theme);
    }
    emit("PARTBLACK complete\r\n");
    return 0;
}
