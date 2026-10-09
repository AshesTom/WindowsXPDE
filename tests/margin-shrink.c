/* Draw theme parts narrower than their sizing margins and print the pixels.
 * Usage: margin-shrink [COM1]. Uses public uxtheme calls only; the parts are
 * Luna's toolbar separators and rebar gripper, compared with XP's output. */
#include <windows.h>
#include <uxtheme.h>
#include <stdio.h>
#include <string.h>
static HANDLE out = INVALID_HANDLE_VALUE;
static void emit(const char *t) { DWORD w; if (out == INVALID_HANDLE_VALUE) fputs(t, stdout); else WriteFile(out, t, (DWORD)strlen(t), &w, NULL); }
int main(int argc, char **argv)
{
    HWND hwnd; HTHEME t; char line[256];
    static const struct { const char *cls; int part, state; } parts[] = {{"Toolbar", 5, 1}, {"Toolbar", 6, 1}, {"Rebar", 1, 0}};
    if (argc > 1)
    {
        out = CreateFileA(argv[1], GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
        if (out != INVALID_HANDLE_VALUE) { DCB d = {.DCBlength = sizeof(d)}; GetCommState(out, &d); d.BaudRate = CBR_115200; d.ByteSize = 8; d.Parity = NOPARITY; d.StopBits = ONESTOPBIT; SetCommState(out, &d); }
    }
    hwnd = CreateWindowA("STATIC", "", WS_POPUP, 0, 0, 1, 1, NULL, NULL, NULL, NULL);
    emit("SEPDRAW\r\n");
    for (unsigned p = 0; p < sizeof(parts) / sizeof(parts[0]); p++)
    {
        WCHAR cls[32]; MultiByteToWideChar(CP_ACP, 0, parts[p].cls, -1, cls, 32);
        t = OpenThemeData(hwnd, cls);
        if (!t) continue;
        for (int w = 1; w <= 12; w++)
        {
            int h = 12; BITMAPINFO bi = {0}; DWORD *bits; RECT r = {2, 1, 2 + w, 1 + 10};
            bi.bmiHeader.biSize = sizeof(bi.bmiHeader); bi.bmiHeader.biWidth = 16; bi.bmiHeader.biHeight = -h;
            bi.bmiHeader.biPlanes = 1; bi.bmiHeader.biBitCount = 32;
            HDC dc = CreateCompatibleDC(NULL); HBITMAP b = CreateDIBSection(dc, &bi, DIB_RGB_COLORS, (void **)&bits, NULL, 0);
            int n;
            SelectObject(dc, b);
            for (int i = 0; i < 16 * h; i++) bits[i] = 0xffffff;
            DrawThemeBackground(t, dc, parts[p].part, parts[p].state, &r, NULL); GdiFlush();
            n = sprintf(line, "%s %d w=%d:", parts[p].cls, parts[p].part, w);
            for (int y = 0; y < h; y += 5)
            {
                n += sprintf(line + n, " |");
                for (int x = 0; x < 16; x++) n += sprintf(line + n, "%c", (bits[y * 16 + x] & 0xffffff) == 0xffffff ? '.' : ((bits[y * 16 + x] & 0xff) < 0xd0 ? '#' : '+'));
            }
            sprintf(line + n, "\r\n"); emit(line);
            DeleteDC(dc); DeleteObject(b);
        }
        CloseThemeData(t);
    }
    emit("SEPDRAW complete\r\n");
    return 0;
}
