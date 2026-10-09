/* Print the pixels of text drawn with a requested font.
 * Usage: text-mask FACE HEIGHT WEIGHT ITALIC TEXT [QUALITY]
 * TEXT may use \uXXXX escapes. Prints '#' for text-colored pixels; used to
 * compare synthetic styles. */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>

int wmain(int argc, WCHAR **argv)
{
    BITMAPINFO info = {0};
    DWORD *bits;
    HDC dc;
    HBITMAP bitmap;
    HFONT font;
    SIZE size;
    int width = 160, height = 40;

    WCHAR text[256];
    int length = 0;

    if (argc < 6) return 1;
    for (const WCHAR *p = argv[5]; *p && length < 255; p++)
    {
        if (p[0] == '\\' && p[1] == 'u' && p[2] && p[3] && p[4] && p[5])
        {
            WCHAR hex[5] = {p[2], p[3], p[4], p[5], 0};
            text[length++] = (WCHAR)wcstoul(hex, NULL, 16);
            p += 5;
        }
        else text[length++] = *p;
    }
    text[length] = 0;
    dc = CreateCompatibleDC(NULL);
    info.bmiHeader.biSize = sizeof(info.bmiHeader);
    info.bmiHeader.biWidth = width;
    info.bmiHeader.biHeight = -height;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    bitmap = CreateDIBSection(dc, &info, DIB_RGB_COLORS, (void **)&bits, NULL, 0);
    SelectObject(dc, bitmap);
    for (int i = 0; i < width * height; i++) bits[i] = 0xffffff;
    font = CreateFontW(_wtoi(argv[2]), 0, 0, 0, _wtoi(argv[3]), _wtoi(argv[4]), FALSE, FALSE, DEFAULT_CHARSET,
                       OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, argc > 6 ? _wtoi(argv[6]) : NONANTIALIASED_QUALITY,
                       DEFAULT_PITCH, argv[1]);
    SelectObject(dc, font);
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, 0);
    GetTextExtentPoint32W(dc, text, length, &size);
    TextOutW(dc, 4, 4, text, length);
    GdiFlush();
    printf("extent=%ldx%ld\n", size.cx, size.cy);
    for (int y = 0; y < height; y++)
    {
        for (int x = 0; x < width; x++)
        {
            DWORD v = bits[y * width + x] & 0xff;
            putchar(v == 0xff ? '.' : v < 0x40 ? '#' : v < 0xc0 ? '+' : '-');
        }
        putchar('\n');
    }
    return 0;
}
