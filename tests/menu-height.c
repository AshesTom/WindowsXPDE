/* The menu bar height (iMenuHeight) that XP keeps under a visual style (patch 0071).
 * Usage: menu-height [COM1|FILE]. Prints the current metrics, then with the flat menu
 * setting toggled, then the smallest height SPI_SETNONCLIENTMETRICS keeps for some menu
 * fonts. Every change is made without saving and is undone before the end. */
#include <windows.h>
#include <stdio.h>
#include <stddef.h>

static FILE *out;
/* XP's NONCLIENTMETRICS has no iPaddedBorderWidth. */
static const UINT size = offsetof(NONCLIENTMETRICSW, iPaddedBorderWidth);

static void get(NONCLIENTMETRICSW *n)
{
    memset(n, 0, sizeof(*n));
    n->cbSize = size;
    SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, size, n, 0);
}

static void show(const char *label)
{
    NONCLIENTMETRICSW n;
    BOOL flat = FALSE;
    HDC dc = GetDC(NULL);
    get(&n);
    SystemParametersInfoW(SPI_GETFLATMENU, 0, &flat, 0);
    fprintf(out, "%s dpi=%d flat=%d menu=%dx%d SM_CYMENU=%d SM_CYMENUSIZE=%d menufont=%ls %ld\r\n", label,
            GetDeviceCaps(dc, LOGPIXELSY), flat, n.iMenuWidth, n.iMenuHeight, GetSystemMetrics(SM_CYMENU),
            GetSystemMetrics(SM_CYMENUSIZE), n.lfMenuFont.lfFaceName, n.lfMenuFont.lfHeight);
    ReleaseDC(NULL, dc);
}

static void smallest(const WCHAR *face, LONG height)
{
    NONCLIENTMETRICSW n;
    TEXTMETRICW tm;
    HDC dc = GetDC(NULL);
    HFONT font, old;
    get(&n);
    lstrcpyW(n.lfMenuFont.lfFaceName, face);
    n.lfMenuFont.lfHeight = height;
    n.iMenuHeight = 2;
    SystemParametersInfoW(SPI_SETNONCLIENTMETRICS, size, &n, 0);
    get(&n);
    font = CreateFontIndirectW(&n.lfMenuFont);
    old = SelectObject(dc, font);
    GetTextMetricsW(dc, &tm);
    SelectObject(dc, old);
    DeleteObject(font);
    ReleaseDC(NULL, dc);
    fprintf(out, "smallest %ls %ld: menu height %d, tmHeight %ld, external leading %ld\r\n", face, height,
            n.iMenuHeight, tm.tmHeight, tm.tmExternalLeading);
}

int main(int argc, char **argv)
{
    NONCLIENTMETRICSW saved;
    BOOL flat = FALSE;
    if (!(out = fopen(argc > 1 ? argv[1] : "com1", "wb"))) return 1;
    show("current");
    SystemParametersInfoW(SPI_GETFLATMENU, 0, &flat, 0);
    SystemParametersInfoW(SPI_SETFLATMENU, 0, (void *)(INT_PTR)!flat, 0);
    show("flat toggled");
    SystemParametersInfoW(SPI_SETFLATMENU, 0, (void *)(INT_PTR)flat, 0);
    get(&saved);
    smallest(L"Tahoma", -11);
    smallest(L"Tahoma", -13);
    smallest(L"Tahoma", -16);
    smallest(L"MS UI Gothic", -12);
    SystemParametersInfoW(SPI_SETNONCLIENTMETRICS, size, &saved, 0);
    show("restored");
    fclose(out);
    return 0;
}
