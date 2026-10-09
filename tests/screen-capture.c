/* Read back screen pixels through the public screen DC.
 * Usage: screen-capture X Y [X Y...]; prints GetPixel and BitBlt results. */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>

int wmain(int argc, WCHAR **argv)
{
    HDC screen = GetDC(NULL), memory = CreateCompatibleDC(screen);
    int width = GetSystemMetrics(SM_CXSCREEN), height = GetSystemMetrics(SM_CYSCREEN);
    HBITMAP bitmap = CreateCompatibleBitmap(screen, width, height);
    HGDIOBJ old = SelectObject(memory, bitmap);
    BOOL blt = BitBlt(memory, 0, 0, width, height, screen, 0, 0, SRCCOPY);
    printf("screen=%dx%d bitblt=%d\n", width, height, blt);
    for (int i = 1; i + 1 < argc; i += 2)
    {
        int x = _wtoi(argv[i]), y = _wtoi(argv[i + 1]);
        printf("pixel %d,%d screen=%06lx copy=%06lx\n", x, y, (unsigned long)GetPixel(screen, x, y),
               (unsigned long)GetPixel(memory, x, y));
    }
    SelectObject(memory, old);
    DeleteObject(bitmap);
    DeleteDC(memory);
    ReleaseDC(NULL, screen);
    return 0;
}
