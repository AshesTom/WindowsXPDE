/* Self-authored pixels: a shrunk TransparentBlt must sample whole source
 * pixels instead of combining dropped rows or columns. */
#include <windows.h>
#include <stdio.h>

#define KEY RGB(255, 0, 255)

static HBITMAP dib(HDC dc, int width, int height, DWORD **bits)
{
    BITMAPINFO info = {0};
    info.bmiHeader.biSize = sizeof(info.bmiHeader);
    info.bmiHeader.biWidth = width;
    info.bmiHeader.biHeight = -height;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    return CreateDIBSection(dc, &info, DIB_RGB_COLORS, (void **)bits, NULL, 0);
}

static DWORD source_color(int x, int y)
{
    static const DWORD colors[] = {0x439543, 0x3f9640, 0x3e973e, 0x80c0ff, 0x204060, 0xc08040};
    if (x == 0 && y == 2) return 0xff00ff;
    return colors[(x * 5 + y * 3) % 6];
}

static int run(int mode)
{
    const int sw = 12, sh = 6, dw = 7, dh = 3;
    HDC src = CreateCompatibleDC(NULL), dst = CreateCompatibleDC(NULL);
    DWORD *src_bits, *dst_bits;
    HBITMAP sb = dib(src, sw, sh, &src_bits), db = dib(dst, dw, dh, &dst_bits);
    int bad = 0;

    SelectObject(src, sb);
    SelectObject(dst, db);
    for (int y = 0; y < sh; y++)
        for (int x = 0; x < sw; x++) src_bits[y * sw + x] = source_color(x, y);
    for (int i = 0; i < dw * dh; i++) dst_bits[i] = 0x123456;
    if (mode) SetStretchBltMode(dst, mode);
    if (!TransparentBlt(dst, 0, 0, dw, dh, src, 0, 0, sw, sh, KEY)) return 100;
    GdiFlush();
    for (int i = 0; i < dw * dh; i++)
    {
        DWORD value = dst_bits[i] & 0xffffff;
        BOOL found = value == 0x123456;
        for (int y = 0; y < sh && !found; y++)
            for (int x = 0; x < sw && !found; x++)
                found = value == source_color(x, y) && value != 0xff00ff;
        if (!found)
        {
            printf("mode=%d pixel=%d,%d value=%06lx\n", mode, i % dw, i / dw, (unsigned long)value);
            bad++;
        }
    }
    DeleteDC(src);
    DeleteDC(dst);
    DeleteObject(sb);
    DeleteObject(db);
    printf("mode=%d bad=%d\n", mode, bad);
    return bad;
}

int wmain(void)
{
    int bad = run(0) + run(HALFTONE) + run(COLORONCOLOR) + run(BLACKONWHITE);
    puts(bad ? "FAIL" : "PASS");
    return bad != 0;
}
