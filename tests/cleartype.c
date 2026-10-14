/* Font smoothing settings and sample text, to compare ClearType with XP.
 *
 *   cleartype get [OUT]                         print the SPI values and the registry values
 *   cleartype set off|standard|cleartype [OUT]  change them as Display Properties does, then print
 *   cleartype sheet ja|en PAGE SECONDS          show sample rows in a 640x480 popup at 0,0
 *   cleartype extents ja|en [OUT]               print each row's GetTextExtentPoint32 width
 *
 * OUT may be COM1 on XP. The sheet closes after SECONDS. */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>

#ifndef SPI_GETFONTSMOOTHINGTYPE
#define SPI_GETFONTSMOOTHINGTYPE 0x200a
#define SPI_SETFONTSMOOTHINGTYPE 0x200b
#define SPI_GETFONTSMOOTHINGCONTRAST 0x200c
#endif
#ifndef SPI_GETFONTSMOOTHINGORIENTATION
#define SPI_GETFONTSMOOTHINGORIENTATION 0x2012
#endif
#ifndef CLEARTYPE_QUALITY
#define CLEARTYPE_QUALITY 5
#endif

struct row
{
    const WCHAR *face;
    int height, weight, italic, quality;
    COLORREF fg, bg;
    const WCHAR *text;
};

#define JA L"\x30d5\x30a1\x30a4\x30eb(F) \x7de8\x96c6(E) \x8868\x793a(V) \x30b9\x30bf\x30fc\x30c8 Recycle Bin 0123"
#define EN L"File Edit View Favorites Tools Help - Recycle Bin, My Computer 0123"
#define WHITE RGB(255, 255, 255)
#define BLACK RGB(0, 0, 0)
#define FACE RGB(236, 233, 216)
#define CAPTION RGB(0, 84, 227)
#define D DEFAULT_QUALITY

static const struct row ja_rows[] =
{
    {L"MS UI Gothic", -12, 400, 0, D, BLACK, WHITE, JA},
    {L"MS UI Gothic", -12, 700, 0, D, BLACK, WHITE, JA},
    {L"MS UI Gothic", -12, 400, 0, D, BLACK, FACE, JA},
    {L"MS UI Gothic", -13, 700, 0, D, WHITE, CAPTION, JA},
    {L"MS UI Gothic", -12, 400, 0, D, RGB(128, 128, 128), WHITE, JA},
    {L"MS UI Gothic", -12, 400, 0, D, WHITE, RGB(49, 106, 197), JA},
    {L"MS UI Gothic", -16, 400, 0, D, BLACK, WHITE, JA},
    {L"MS UI Gothic", -19, 700, 0, D, BLACK, WHITE, JA},
    {L"MS UI Gothic", -22, 400, 0, D, BLACK, WHITE, JA},
    {L"MS UI Gothic", -23, 400, 0, D, BLACK, WHITE, JA},
    {L"MS UI Gothic", -24, 400, 0, D, BLACK, WHITE, JA},
    {L"MS UI Gothic", -25, 400, 0, D, BLACK, WHITE, JA},
    {L"MS UI Gothic", -12, 400, 0, NONANTIALIASED_QUALITY, BLACK, WHITE, JA},
    {L"MS UI Gothic", -12, 400, 0, ANTIALIASED_QUALITY, BLACK, WHITE, JA},
    {L"MS UI Gothic", -12, 400, 0, CLEARTYPE_QUALITY, BLACK, WHITE, JA},
    {L"MS UI Gothic", -12, 400, 1, D, BLACK, WHITE, JA},
    {L"MS Gothic", -12, 400, 0, D, BLACK, WHITE, JA},
    {L"MS PGothic", -12, 400, 0, D, BLACK, WHITE, JA},
    {L"MS Mincho", -12, 400, 0, D, BLACK, WHITE, JA},
    {L"MS PGothic", -32, 400, 0, D, BLACK, WHITE, JA},
    {L"Tahoma", -11, 400, 0, D, BLACK, WHITE, EN},
    {L"Arial", -12, 400, 0, D, BLACK, WHITE, EN},
    {L"Arial", -18, 400, 0, D, BLACK, WHITE, EN},
    {L"Trebuchet MS", -13, 700, 0, D, WHITE, CAPTION, EN},
};

static const struct row en_rows[] =
{
    {L"Tahoma", -11, 400, 0, D, BLACK, WHITE, EN},
    {L"Tahoma", -11, 700, 0, D, BLACK, WHITE, EN},
    {L"Tahoma", -11, 400, 0, D, BLACK, FACE, EN},
    {L"Trebuchet MS", -13, 700, 0, D, WHITE, CAPTION, EN},
    {L"Tahoma", -11, 400, 0, D, RGB(128, 128, 128), WHITE, EN},
    {L"Tahoma", -11, 400, 0, D, WHITE, RGB(49, 106, 197), EN},
    {L"Franklin Gothic Medium", -19, 400, 1, D, WHITE, RGB(56, 145, 56), EN},
    {L"Tahoma", -13, 700, 0, D, BLACK, WHITE, EN},
    {L"Tahoma", -19, 700, 0, D, BLACK, WHITE, EN},
    {L"Microsoft Sans Serif", -11, 400, 0, D, BLACK, WHITE, EN},
    {L"Tahoma", -11, 400, 0, NONANTIALIASED_QUALITY, BLACK, WHITE, EN},
    {L"Tahoma", -11, 400, 0, ANTIALIASED_QUALITY, BLACK, WHITE, EN},
    {L"Tahoma", -11, 400, 0, CLEARTYPE_QUALITY, BLACK, WHITE, EN},
    {L"Tahoma", -11, 400, 1, D, BLACK, WHITE, EN},
    {L"Arial", -12, 400, 0, D, BLACK, WHITE, EN},
    {L"Arial", -17, 400, 0, D, BLACK, WHITE, EN},
    {L"Arial", -18, 400, 0, D, BLACK, WHITE, EN},
    {L"Verdana", -11, 400, 0, D, BLACK, WHITE, EN},
    {L"Times New Roman", -16, 400, 0, D, BLACK, WHITE, EN},
    {L"Courier New", -13, 400, 0, D, BLACK, WHITE, EN},
    {L"Lucida Console", -12, 400, 0, D, BLACK, WHITE, EN},
    {L"Tahoma", -32, 400, 0, D, BLACK, WHITE, EN},
};

static const struct row *rows;
static int first, count;

static int row_height(const struct row *row)
{
    return -row->height + 7;
}

static void paint(HDC dc)
{
    RECT rect = {0, 0, 640, 480};
    int y = 0;

    FillRect(dc, &rect, GetStockObject(WHITE_BRUSH));
    for (int i = first; i < count; i++)
    {
        const struct row *row = &rows[i];
        HFONT font = CreateFontW(row->height, 0, 0, 0, row->weight, row->italic, FALSE, FALSE, DEFAULT_CHARSET,
                                 OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, row->quality, DEFAULT_PITCH, row->face);
        HBRUSH brush = CreateSolidBrush(row->bg);
        HGDIOBJ old;

        if (y + row_height(row) > 480) break;
        rect.top = y;
        rect.bottom = y + row_height(row);
        FillRect(dc, &rect, brush);
        DeleteObject(brush);
        old = SelectObject(dc, font);
        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, row->fg);
        TextOutW(dc, 4, y + 3, row->text, lstrlenW(row->text));
        SelectObject(dc, old);
        DeleteObject(font);
        y = rect.bottom;
    }
}

static LRESULT CALLBACK proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
    PAINTSTRUCT ps;

    switch (msg)
    {
    case WM_PAINT:
        paint(BeginPaint(hwnd, &ps));
        EndPaint(hwnd, &ps);
        return 0;
    case WM_TIMER:
        DestroyWindow(hwnd);
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wparam, lparam);
}

static void print(FILE *out)
{
    UINT smoothing = 0, type = 0, contrast = 0, orientation = 0;
    HKEY key;

    SystemParametersInfoW(SPI_GETFONTSMOOTHING, 0, &smoothing, 0);
    SystemParametersInfoW(SPI_GETFONTSMOOTHINGTYPE, 0, &type, 0);
    SystemParametersInfoW(SPI_GETFONTSMOOTHINGCONTRAST, 0, &contrast, 0);
    if (!SystemParametersInfoW(SPI_GETFONTSMOOTHINGORIENTATION, 0, &orientation, 0)) orientation = 99;
    fprintf(out, "spi smoothing=%u type=%u contrast=%u orientation=%u\r\n", smoothing, type, contrast, orientation);
    if (!RegOpenKeyExW(HKEY_CURRENT_USER, L"Control Panel\\Desktop", 0, KEY_READ, &key))
    {
        static const WCHAR *names[] = {L"FontSmoothing", L"FontSmoothingType", L"FontSmoothingGamma",
                                       L"FontSmoothingOrientation"};
        for (int i = 0; i < 4; i++)
        {
            BYTE data[64];
            DWORD type, size = sizeof(data) - 2;
            LONG err = RegQueryValueExW(key, names[i], NULL, &type, data, &size);
            if (err) fprintf(out, "%ls missing\r\n", names[i]);
            else if (type == REG_DWORD) fprintf(out, "%ls dword %lu\r\n", names[i], *(DWORD *)data);
            else
            {
                data[size] = data[size + 1] = 0;
                fprintf(out, "%ls type %lu \"%ls\"\r\n", names[i], type, (WCHAR *)data);
            }
        }
        RegCloseKey(key);
    }
}

int wmain(int argc, WCHAR **argv)
{
    FILE *out = stdout;

    if (argc >= 2 && !lstrcmpW(argv[1], L"get"))
    {
        if (argc > 2) out = _wfopen(argv[2], L"wb");
        print(out);
        fclose(out);
        return 0;
    }
    if (argc >= 3 && !lstrcmpW(argv[1], L"set"))
    {
        UINT flags = SPIF_UPDATEINIFILE | SPIF_SENDCHANGE;
        BOOL on = lstrcmpW(argv[2], L"off") != 0;

        if (argc > 3) out = _wfopen(argv[3], L"wb");
        SystemParametersInfoW(SPI_SETFONTSMOOTHING, on, NULL, flags);
        if (on) SystemParametersInfoW(SPI_SETFONTSMOOTHINGTYPE, 0,
                                      (void *)(UINT_PTR)(lstrcmpW(argv[2], L"cleartype") ? 1 : 2), flags);
        print(out);
        fclose(out);
        return 0;
    }
    if (argc >= 3 && !lstrcmpW(argv[1], L"extents"))
    {
        HDC dc = GetDC(NULL);

        rows = lstrcmpW(argv[2], L"en") ? ja_rows : en_rows;
        count = rows == ja_rows ? ARRAYSIZE(ja_rows) : ARRAYSIZE(en_rows);
        if (argc > 3) out = _wfopen(argv[3], L"wb");
        for (int i = 0; i < count; i++)
        {
            const struct row *row = &rows[i];
            HFONT font = CreateFontW(row->height, 0, 0, 0, row->weight, row->italic, FALSE, FALSE, DEFAULT_CHARSET,
                                     OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, row->quality, DEFAULT_PITCH, row->face);
            HGDIOBJ old = SelectObject(dc, font);
            SIZE size;

            GetTextExtentPoint32W(dc, row->text, lstrlenW(row->text), &size);
            fprintf(out, "%2d %ls %d %d %ld\r\n", i, row->face, row->height, row->weight, size.cx);
            SelectObject(dc, old);
            DeleteObject(font);
        }
        ReleaseDC(NULL, dc);
        fclose(out);
        return 0;
    }
    if (argc >= 5 && !lstrcmpW(argv[1], L"sheet"))
    {
        WNDCLASSW cls = {0};
        MSG msg;
        HWND hwnd;
        int page = _wtoi(argv[3]), y = 0;

        rows = lstrcmpW(argv[2], L"en") ? ja_rows : en_rows;
        count = rows == ja_rows ? ARRAYSIZE(ja_rows) : ARRAYSIZE(en_rows);
        /* Pages split the rows where the 480 pixels end. */
        for (first = 0; page && first < count; first++)
        {
            if (y + row_height(&rows[first]) > 480) { page--; y = 0; if (!page) break; }
            y += row_height(&rows[first]);
        }
        cls.lpfnWndProc = proc;
        cls.hInstance = GetModuleHandleW(NULL);
        cls.lpszClassName = L"cleartype-sheet";
        RegisterClassW(&cls);
        hwnd = CreateWindowExW(WS_EX_TOPMOST | WS_EX_TOOLWINDOW, cls.lpszClassName, L"sheet", WS_POPUP | WS_VISIBLE,
                               0, 0, 640, 480, NULL, NULL, cls.hInstance, NULL);
        SetTimer(hwnd, 1, _wtoi(argv[4]) * 1000, NULL);
        while (GetMessageW(&msg, NULL, 0, 0) > 0) DispatchMessageW(&msg);
        return 0;
    }
    return 1;
}
