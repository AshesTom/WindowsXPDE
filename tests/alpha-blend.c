/* Print AlphaBlend's result for one premultiplied source pixel over one
 * destination pixel, over a grid of alpha, source and destination values.
 * Usage: alpha-blend [COM1]. Self-authored 1x1 bitmaps only. */
#include <windows.h>
#include <stdio.h>
#include <string.h>

static HANDLE out = INVALID_HANDLE_VALUE;

static void emit(const char *text)
{
    DWORD written;
    if (out == INVALID_HANDLE_VALUE) fputs(text, stdout);
    else WriteFile(out, text, (DWORD)strlen(text), &written, NULL);
}

static HDC pixel(DWORD **bits)
{
    BITMAPINFO info = {0};
    HDC dc = CreateCompatibleDC(NULL);
    info.bmiHeader.biSize = sizeof(info.bmiHeader);
    info.bmiHeader.biWidth = 1;
    info.bmiHeader.biHeight = 1;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    SelectObject(dc, CreateDIBSection(dc, &info, DIB_RGB_COLORS, (void **)bits, NULL, 0));
    return dc;
}

int main(int argc, char **argv)
{
    BLENDFUNCTION blend = {AC_SRC_OVER, 0, 255, AC_SRC_ALPHA};
    DWORD *src_bits, *dst_bits;
    HDC src = pixel(&src_bits), dst = pixel(&dst_bits);
    char line[2048];

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
    emit("ALPHABLEND\r\n");
    for (int a = 1; a < 255; a += 7)
    {
        /* blue = source s, green = s / 2, red = 0; destination d in every channel */
        for (int s = 0; s <= a; s += a / 3 + 1)
        {
            int n = sprintf(line, "a%d s%d:", a, s);
            for (int d = 0; d < 256; d += 15)
            {
                *src_bits = (a << 24) | ((s / 2) << 8) | s;
                *dst_bits = (d << 16) | (d << 8) | d;
                GdiAlphaBlend(dst, 0, 0, 1, 1, src, 0, 0, 1, 1, blend);
                GdiFlush();
                n += sprintf(line + n, " %06lx", *dst_bits & 0xffffff);
            }
            sprintf(line + n, "\r\n");
            emit(line);
        }
    }
    emit("ALPHABLEND complete\r\n");
    return 0;
}
