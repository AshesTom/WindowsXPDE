/* Print Marlett caption glyph masks for several public font/placement choices.
 * Usage: marlett-caption WIDTH HEIGHT [FONT_HEIGHT...] | sweep. The output is compared with a
 * screenshot of the same classic caption button; no font data is exported. */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>

static void show(const char *label, int width, int height, int font_height, BOOL drawtext)
{
    static const WCHAR glyphs[] = {'0', '1', '2', 'r'};
    BITMAPINFO info = {0};
    DWORD *bits;
    HDC dc = CreateCompatibleDC(NULL);
    HBITMAP bitmap;
    HFONT font, old_font;
    int total = width * 4;

    info.bmiHeader.biSize = sizeof(info.bmiHeader);
    info.bmiHeader.biWidth = total;
    info.bmiHeader.biHeight = -height;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    bitmap = CreateDIBSection(dc, &info, DIB_RGB_COLORS, (void **)&bits, NULL, 0);
    SelectObject(dc, bitmap);
    for (int i = 0; i < total * height; i++) bits[i] = 0xffffff;
    font = CreateFontW(font_height, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, SYMBOL_CHARSET,
                       OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
                       FIXED_PITCH | FF_DONTCARE, L"Marlett");
    old_font = SelectObject(dc, font);
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, 0);
    for (int g = 0; g < 4; g++)
    {
        RECT r = {g * width, 0, g * width + width, height}, square = r;
        int side = min(width, height);
        SIZE size;
        square.left = r.left + (width - side) / 2;
        square.right = square.left + side;
        square.top = (height - side) / 2;
        square.bottom = square.top + side;
        if (drawtext)
        {
            InflateRect(&square, -1, -1);
            DrawTextW(dc, &glyphs[g], 1, &square, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOCLIP);
        }
        else
        {
            int xc = (square.left + square.right) / 2, yc = (square.top + square.bottom) / 2;
            GetTextExtentPoint32W(dc, &glyphs[g], 1, &size);
            TextOutW(dc, xc - size.cx / 2, yc - size.cy / 2, &glyphs[g], 1);
        }
    }
    GdiFlush();
    printf("%s height=%d\n", label, font_height);
    for (int y = 0; y < height; y++)
    {
        for (int x = 0; x < total; x++) putchar((bits[y * total + x] & 0xffffff) == 0 ? '#' : '.');
        putchar('\n');
    }
    SelectObject(dc, old_font);
    DeleteObject(font);
    DeleteObject(bitmap);
    DeleteDC(dc);
}

/* Print each glyph at every em height from a fixed TextOut origin. */
static void sweep(void)
{
    static const WCHAR glyphs[] = {'0', '1', '2', 'r', 's'};
    BITMAPINFO info = {0};
    DWORD *bits;
    HDC dc = CreateCompatibleDC(NULL);
    HBITMAP bitmap;
    info.bmiHeader.biSize = sizeof(info.bmiHeader);
    info.bmiHeader.biWidth = 64;
    info.bmiHeader.biHeight = -64;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    bitmap = CreateDIBSection(dc, &info, DIB_RGB_COLORS, (void **)&bits, NULL, 0);
    SelectObject(dc, bitmap);
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, 0);
    for (int em = 4; em <= 32; em++)
    {
        HFONT font = CreateFontW(-em, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, SYMBOL_CHARSET,
                                 OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
                                 FIXED_PITCH | FF_DONTCARE, L"Marlett");
        HGDIOBJ old = SelectObject(dc, font);
        for (int g = 0; g < 5; g++)
        {
            SIZE size;
            int x0 = 64, y0 = 64, x1 = -1, y1 = -1;
            for (int i = 0; i < 64 * 64; i++) bits[i] = 0xffffff;
            GetTextExtentPoint32W(dc, &glyphs[g], 1, &size);
            TextOutW(dc, 8, 8, &glyphs[g], 1);
            GdiFlush();
            for (int y = 0; y < 64; y++)
                for (int x = 0; x < 64; x++)
                    if (!(bits[y * 64 + x] & 0xffffff))
                    {
                        x0 = min(x0, x); y0 = min(y0, y); x1 = max(x1, x); y1 = max(y1, y);
                    }
            printf("em %d glyph %c extent %ldx%ld bbox %d %d %d %d\n", em, (char)glyphs[g],
                   size.cx, size.cy, x0 - 8, y0 - 8, x1 - 8, y1 - 8);
            for (int y = y0; y <= y1 && x1 >= 0; y++)
            {
                for (int x = x0; x <= x1; x++) putchar(bits[y * 64 + x] & 0xffffff ? '.' : '#');
                putchar('\n');
            }
        }
        SelectObject(dc, old);
        DeleteObject(font);
    }
    DeleteObject(bitmap);
    DeleteDC(dc);
}

int wmain(int argc, WCHAR **argv)
{
    if (argc > 1 && !wcscmp(argv[1], L"sweep"))
    {
        sweep();
        return 0;
    }
    int width = argc > 1 ? _wtoi(argv[1]) : 16, height = argc > 2 ? _wtoi(argv[2]) : 14;
    int side = min(width, height);
    if (argc > 3)
    {
        for (int i = 3; i < argc; i++) show("textout", width, height, _wtoi(argv[i]), FALSE);
        return 0;
    }
    show("wine-textout-em", width, height, -(side - 2), FALSE);
    show("textout-cell", width, height, side - 2, FALSE);
    show("drawtext-cell", width, height, side - 2, TRUE);
    show("drawtext-em", width, height, -(side - 2), TRUE);
    return 0;
}
