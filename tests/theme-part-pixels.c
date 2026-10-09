/* Draw one theme part with the public API and print its properties and pixels.
 * Usage: theme-part-pixels CLASS SUBAPP|- PART STATE WIDTH HEIGHT BG_RRGGBB
 * The background is filled first so partially transparent parts can be
 * compared with a screenshot of the same part on the same background.
 * Nothing is written except standard output; no theme resource is exported. */
#include <windows.h>
#include <uxtheme.h>
#include <vssym32.h>
#include <stdio.h>
#include <stdlib.h>

static void print_enum(HTHEME theme, int part, int state, int prop, const char *name)
{
    int value = -1;
    HRESULT hr = GetThemeEnumValue(theme, part, state, prop, &value);
    printf("%s=%d hr=%08lx\n", name, value, (unsigned long)hr);
}

static void print_int(HTHEME theme, int part, int state, int prop, const char *name)
{
    int value = -1;
    HRESULT hr = GetThemeInt(theme, part, state, prop, &value);
    printf("%s=%d hr=%08lx\n", name, value, (unsigned long)hr);
}

int wmain(int argc, WCHAR **argv)
{
    WNDCLASSW wc = {0};
    BITMAPINFO info = {0};
    HWND hwnd;
    HTHEME theme;
    HDC dc;
    HBITMAP bitmap;
    HGDIOBJ old;
    DWORD *bits;
    MARGINS margins;
    SIZE size;
    RECT rect;
    BOOL value;
    COLORREF color;
    HRESULT hr;
    int part, state, width, height;
    unsigned int bg;

    if (argc < 8)
    {
        fputs("usage: CLASS SUBAPP|- PART STATE WIDTH HEIGHT BG_RRGGBB\n", stderr);
        return 1;
    }
    part = _wtoi(argv[3]);
    state = _wtoi(argv[4]);
    width = _wtoi(argv[5]);
    height = _wtoi(argv[6]);
    bg = wcstoul(argv[7], NULL, 16);
    if (width <= 0 || height <= 0 || width > 2048 || height > 2048) return 1;

    wc.lpfnWndProc = DefWindowProcW;
    wc.hInstance = GetModuleHandleW(NULL);
    wc.lpszClassName = L"ThemePartPixels";
    if (!RegisterClassW(&wc)) return 2;
    hwnd = CreateWindowW(wc.lpszClassName, L"theme part", WS_POPUP, 0, 0, 1, 1,
                         NULL, NULL, wc.hInstance, NULL);
    if (!hwnd) return 2;
    if (wcscmp(argv[2], L"-")) SetWindowTheme(hwnd, argv[2], NULL);
    theme = OpenThemeData(hwnd, argv[1]);
    printf("theme=%d\n", theme != NULL);
    if (!theme) return 3;

    print_enum(theme, part, state, TMT_BGTYPE, "bgtype");
    print_enum(theme, part, state, TMT_SIZINGTYPE, "sizingtype");
    print_enum(theme, part, state, TMT_IMAGELAYOUT, "imagelayout");
    print_enum(theme, part, state, TMT_IMAGESELECTTYPE, "imageselecttype");
    print_enum(theme, part, state, TMT_GLYPHTYPE, "glyphtype");
    print_int(theme, part, state, TMT_IMAGECOUNT, "imagecount");
    print_int(theme, part, state, TMT_ALPHATHRESHOLD, "alphathreshold");
    value = -1;
    hr = GetThemeBool(theme, part, state, TMT_TRANSPARENT, &value);
    printf("transparent=%d hr=%08lx\n", value, (unsigned long)hr);
    value = -1;
    hr = GetThemeBool(theme, part, state, TMT_UNIFORMSIZING, &value);
    printf("uniformsizing=%d hr=%08lx\n", value, (unsigned long)hr);
    value = -1;
    hr = GetThemeBool(theme, part, state, TMT_BORDERONLY, &value);
    printf("borderonly=%d hr=%08lx\n", value, (unsigned long)hr);
    color = 0;
    hr = GetThemeColor(theme, part, state, TMT_TRANSPARENTCOLOR, &color);
    printf("transparentcolor=%06lx hr=%08lx\n", (unsigned long)color, (unsigned long)hr);
    printf("partial=%d\n", IsThemeBackgroundPartiallyTransparent(theme, part, state));
    hr = GetThemeMargins(theme, NULL, part, state, TMT_SIZINGMARGINS, NULL, &margins);
    printf("sizingmargins=%d,%d,%d,%d hr=%08lx\n", margins.cxLeftWidth, margins.cxRightWidth,
           margins.cyTopHeight, margins.cyBottomHeight, (unsigned long)hr);
    hr = GetThemeMargins(theme, NULL, part, state, TMT_CONTENTMARGINS, NULL, &margins);
    printf("contentmargins=%d,%d,%d,%d hr=%08lx\n", margins.cxLeftWidth, margins.cxRightWidth,
           margins.cyTopHeight, margins.cyBottomHeight, (unsigned long)hr);
    hr = GetThemePartSize(theme, NULL, part, state, NULL, TS_TRUE, &size);
    printf("truesize=%ldx%ld hr=%08lx\n", size.cx, size.cy, (unsigned long)hr);
    hr = GetThemePartSize(theme, NULL, part, state, NULL, TS_MIN, &size);
    printf("minsize=%ldx%ld hr=%08lx\n", size.cx, size.cy, (unsigned long)hr);

    info.bmiHeader.biSize = sizeof(info.bmiHeader);
    info.bmiHeader.biWidth = width;
    info.bmiHeader.biHeight = -height;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    dc = CreateCompatibleDC(NULL);
    bitmap = CreateDIBSection(dc, &info, DIB_RGB_COLORS, (void **)&bits, NULL, 0);
    if (!dc || !bitmap) return 4;
    old = SelectObject(dc, bitmap);
    for (int i = 0; i < width * height; i++)
        bits[i] = ((bg >> 16) & 0xff) << 16 | ((bg >> 8) & 0xff) << 8 | (bg & 0xff);
    SetRect(&rect, 0, 0, width, height);
    hr = DrawThemeBackground(theme, dc, part, state, &rect, NULL);
    GdiFlush();
    printf("draw hr=%08lx\n", (unsigned long)hr);
    for (int y = 0; y < height; y++)
    {
        printf("row %d", y);
        for (int x = 0; x < width; x++) printf(" %06lx", (unsigned long)(bits[y * width + x] & 0xffffff));
        printf("\n");
    }
    SelectObject(dc, old);
    DeleteObject(bitmap);
    DeleteDC(dc);
    CloseThemeData(theme);
    DestroyWindow(hwnd);
    return 0;
}
