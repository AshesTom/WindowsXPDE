/* Public probe: MS UI Gothic text drawn by ExtTextOut (black on white, no smoothing) at several
 * heights, weights and slants, as hex rows of a 1bpp DIB, to compare XP's and Wine's synthetic bold
 * and italic. Usage: text-bits-dpi.exe OUT. Numbers only. */
#ifndef UNICODE
#define UNICODE
#endif
#include <windows.h>
#include <stdio.h>

int main(int argc, char **argv)
{
    static const int heights[] = { 12, 13, 15, 16, 18, 20, 24 };
    static const WCHAR text[] = { 0x30de, 0x30a4, ' ', 0x30b3, 0x30f3, 'E', 'V', 'A', '(', ')', 0 };
    FILE *out = fopen(argc > 1 ? argv[1] : "com1", "wb");
    HDC screen = GetDC(NULL), dc = CreateCompatibleDC(screen);
    struct { BITMAPINFOHEADER h; RGBQUAD colors[2]; } info = {{sizeof(BITMAPINFOHEADER), 160, -32, 1, 1, BI_RGB, 0, 0, 0, 2, 2},
                                                             {{0, 0, 0, 0}, {255, 255, 255, 0}}};
    BYTE *bits;
    HBITMAP bitmap = CreateDIBSection(screen, (BITMAPINFO *)&info, DIB_RGB_COLORS, (void **)&bits, NULL, 0);
    unsigned h, style, row, i;

    if (!out) return 1;
    SelectObject(dc, bitmap);
    for (h = 0; h < sizeof(heights) / sizeof(heights[0]); h++)
        for (style = 0; style < 4; style++)
        {
            HFONT font = CreateFontW(-heights[h], 0, 0, 0, style & 1 ? 700 : 400, style & 2, FALSE, FALSE,
                                     SHIFTJIS_CHARSET, 0, 0, NONANTIALIASED_QUALITY, 0, L"MS UI Gothic");
            HGDIOBJ old = SelectObject(dc, font);
            RECT all = {0, 0, 160, 32};
            SIZE size;
            FillRect(dc, &all, GetStockObject(WHITE_BRUSH));
            SetTextColor(dc, RGB(0, 0, 0));
            SetBkMode(dc, TRANSPARENT);
            ExtTextOutW(dc, 4, 2, 0, NULL, text, lstrlenW(text), NULL);
            GetTextExtentPoint32W(dc, text, lstrlenW(text), &size);
            GdiFlush();
            fprintf(out, "h%d b%u i%u extent %ldx%ld:", heights[h], style & 1, style >> 1, size.cx, size.cy);
            for (row = 0; row < 32; row++)
            {
                fprintf(out, " ");
                for (i = 0; i < 20; i++) fprintf(out, "%02x", bits[row * 20 + i]);
            }
            fprintf(out, "\n");
            DeleteObject(SelectObject(dc, old));
        }
    fclose(out);
    return 0;
}
