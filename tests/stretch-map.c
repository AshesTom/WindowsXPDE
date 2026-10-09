/* Print which source pixel StretchBlt (S) and TransparentBlt (T) take for each
 * destination pixel, with COLORONCOLOR, for sizes 2..9 into 1..12.
 * Usage: stretch-map [COM1|--verify]. Self-authored one-row bitmaps only.
 * --verify checks TransparentBlt against XP's measured rule,
 * floor((2 * d + 1) * src / (2 * dst)), which the external shell follows. */
#include <windows.h>
#include <stdio.h>
#include <string.h>
static HANDLE out = INVALID_HANDLE_VALUE;
static void emit(const char *t) { DWORD w; if (out == INVALID_HANDLE_VALUE) fputs(t, stdout); else WriteFile(out, t, (DWORD)strlen(t), &w, NULL); }
static HDC dib(int w, DWORD **bits)
{
    BITMAPINFO bi = {0};
    bi.bmiHeader.biSize = sizeof(bi.bmiHeader);
    bi.bmiHeader.biWidth = w;
    bi.bmiHeader.biHeight = -1;
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    HDC dc = CreateCompatibleDC(NULL);
    SelectObject(dc, CreateDIBSection(dc, &bi, DIB_RGB_COLORS, (void **)bits, NULL, 0));
    return dc;
}
int main(int argc, char **argv)
{
    char line[512];
    int verify = argc > 1 && !strcmp(argv[1], "--verify"), bad = 0;
    if (argc > 1 && !verify)
    {
        out = CreateFileA(argv[1], GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
        if (out != INVALID_HANDLE_VALUE) { DCB d = {.DCBlength = sizeof(d)}; GetCommState(out, &d); d.BaudRate = CBR_115200; d.ByteSize = 8; d.Parity = NOPARITY; d.StopBits = ONESTOPBIT; SetCommState(out, &d); }
    }
    emit("STRETCHMAP\r\n");
    for (int api = 0; api < 2; api++)
        for (int sw = 2; sw <= 9; sw++)
            for (int dw = 1; dw <= 12; dw++)
            {
                DWORD *sb, *db; HDC s = dib(sw, &sb), d = dib(dw, &db); int n;
                for (int i = 0; i < sw; i++) sb[i] = 0x010101 * (i + 1);
                for (int i = 0; i < dw; i++) db[i] = 0xff00ff;
                SetStretchBltMode(d, COLORONCOLOR);
                if (api) TransparentBlt(d, 0, 0, dw, 1, s, 0, 0, sw, 1, RGB(0, 255, 0));
                else StretchBlt(d, 0, 0, dw, 1, s, 0, 0, sw, 1, SRCCOPY);
                GdiFlush();
                n = sprintf(line, "%s %d->%d:", api ? "T" : "S", sw, dw);
                for (int i = 0; i < dw; i++)
                {
                    n += sprintf(line + n, " %ld", (long)(db[i] & 0xff) - 1);
                    if (verify && api && (long)(db[i] & 0xff) - 1 != (2 * i + 1) * sw / (2 * dw)) bad++;
                }
                sprintf(line + n, "\r\n"); emit(line);
                DeleteDC(s); DeleteDC(d);
            }
    emit("STRETCHMAP complete\r\n");
    if (verify) printf("%s: TransparentBlt mapping, %d mismatches\n", bad ? "FAIL" : "PASS", bad);
    return bad ? 1 : 0;
}
