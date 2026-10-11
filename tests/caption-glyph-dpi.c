/* Public probe: which Luna caption glyph DrawThemeBackground picks for a button of a given size.
 * Usage: caption-glyph-dpi.exe OUT. Prints the content margins and the white glyph's extent for
 * the close and maximize buttons drawn into 21..44 pixel squares. Numbers only. */
#ifndef UNICODE
#define UNICODE
#endif
#include <windows.h>
#include <uxtheme.h>
#include <vssym32.h>
#include <stdio.h>

int main(int argc, char **argv)
{
    static const int parts[2] = { WP_CLOSEBUTTON, WP_MAXBUTTON };
    FILE *out = fopen(argc > 1 ? argv[1] : "com1", "wb");
    HWND hwnd = CreateWindowW(L"Static", L"", WS_POPUP, 0, 0, 10, 10, NULL, NULL, NULL, NULL);
    HTHEME theme = OpenThemeData(hwnd, L"Window");
    HDC screen = GetDC(NULL), dc = CreateCompatibleDC(screen);
    BITMAPINFO info;
    DWORD *bits;
    HBITMAP bitmap;
    int p, size;

    if (!out) return 1;
    memset(&info, 0, sizeof(info));
    info.bmiHeader.biSize = sizeof(info.bmiHeader);
    info.bmiHeader.biWidth = 64;
    info.bmiHeader.biHeight = -64;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;
    bitmap = CreateDIBSection(screen, &info, DIB_RGB_COLORS, (void **)&bits, NULL, 0);
    fprintf(out, "dpi=%d theme=%p\n", GetDeviceCaps(screen, LOGPIXELSY), theme);
    if (!theme) return 0;
    SelectObject(dc, bitmap);
    for (p = 0; p < 2; p++)
    {
        MARGINS margins = {0};
        GetThemeMargins(theme, dc, parts[p], 1, TMT_CONTENTMARGINS, NULL, &margins);
        fprintf(out, "part %d content margins %d,%d,%d,%d\n", parts[p], margins.cxLeftWidth, margins.cyTopHeight,
                margins.cxRightWidth, margins.cyBottomHeight);
        for (size = 21; size <= 44; size++)
        {
            RECT rect = {0, 0, size, size};
            int x, y, left = 64, top = 64, right = -1, bottom = -1;
            memset(bits, 0, 64 * 64 * 4);
            DrawThemeBackground(theme, dc, parts[p], 1, &rect, NULL);
            GdiFlush();
            /* The glyph is the white inside the button, away from its white edge. */
            for (y = 3; y < size - 3; y++)
                for (x = 3; x < size - 3; x++)
                {
                    DWORD c = bits[y * 64 + x];
                    if ((c & 0xff) > 0xe0 && ((c >> 8) & 0xff) > 0xe0 && ((c >> 16) & 0xff) > 0xe0)
                    {
                        if (x < left) left = x;
                        if (x > right) right = x;
                        if (y < top) top = y;
                        if (y > bottom) bottom = y;
                    }
                }
            fprintf(out, "part %d size %d glyph %d,%d..%d,%d (%dx%d)\n", parts[p], size, left, top, right + 1,
                    bottom + 1, right + 1 - left, bottom + 1 - top);
        }
    }
    fclose(out);
    return 0;
}
