/* Print where DrawText draws the access key underline. Usage:
 * text-underline [COM1|FILE]. Each case draws the text with and without
 * the prefix into a DIB section, black on white, and prints the rectangle
 * of the pixels that differ, with the text extents before and through the
 * underlined character. Public user32 and gdi32 calls only. */
#include <windows.h>
#include <stdio.h>
#include <string.h>

static HANDLE out = INVALID_HANDLE_VALUE;

static void emit(const WCHAR *text)
{
    char utf8[1024];
    DWORD written;
    int length = WideCharToMultiByte(CP_UTF8, 0, text, -1, utf8, sizeof(utf8), NULL, NULL);
    if (length <= 1) return;
    if (out == INVALID_HANDLE_VALUE) fputs(utf8, stdout);
    else WriteFile(out, utf8, length - 1, &written, NULL);
}

#define W 200
#define H 40

static DWORD *bits;

static void draw(HDC dc, const WCHAR *text, const RECT *rect, UINT flags, DWORD *copy)
{
    RECT r = *rect;
    PatBlt(dc, 0, 0, W, H, WHITENESS);
    DrawTextW(dc, text, -1, &r, flags);
    GdiFlush();
    memcpy(copy, bits, W * H * 4);
}

static void measure(HDC dc, const WCHAR *font, int height, int weight, BOOL italic,
                    const WCHAR *text, int left, int right, UINT flags)
{
    static DWORD with[W * H], without[W * H];
    WCHAR plain[64], line[512];
    LOGFONTW lf = {0};
    HFONT hfont, old;
    RECT rect = {left, 4, right, H};
    int x0 = W, y0 = H, x1 = -1, y1 = -1, offset = -1, j = 0;
    SIZE before = {0}, through = {0};

    for (int i = 0; text[i]; i++)
    {
        if (text[i] == L'&' && text[i + 1] != L'&' && offset < 0) { offset = j; continue; }
        plain[j++] = text[i];
    }
    plain[j] = 0;
    lf.lfHeight = height;
    lf.lfWeight = weight;
    lf.lfItalic = italic;
    lf.lfCharSet = DEFAULT_CHARSET;
    lstrcpynW(lf.lfFaceName, font, LF_FACESIZE);
    hfont = CreateFontIndirectW(&lf);
    old = SelectObject(dc, hfont);
    if (offset >= 0)
    {
        GetTextExtentPoint32W(dc, plain, offset, &before);
        GetTextExtentPoint32W(dc, plain, offset + 1, &through);
    }
    draw(dc, text, &rect, flags, with);
    draw(dc, plain, &rect, flags | DT_NOPREFIX, without);
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++)
            if (with[y * W + x] != without[y * W + x])
            {
                if (x < x0) x0 = x;
                if (x > x1) x1 = x;
                if (y < y0) y0 = y;
                if (y > y1) y1 = y;
            }
    swprintf(line, ARRAYSIZE(line), L"%ls %d w%d i%d '%ls' rect=%d..%d flags=%x before=%ld through=%ld underline=",
             font, height, weight, italic, text, left, right, flags, before.cx, through.cx);
    emit(line);
    if (x1 < 0) swprintf(line, ARRAYSIZE(line), L"none\r\n");
    else swprintf(line, ARRAYSIZE(line), L"x%d..%d y%d..%d\r\n", x0, x1, y0, y1);
    emit(line);
    SelectObject(dc, old);
    DeleteObject(hfont);
}

int wmain(int argc, WCHAR **argv)
{
    static const WCHAR *texts[] = {
        L"&A", L"a&Ib", L"(&E)", L"(&W)", L"&i", L"W&W", L"\x65b0\x898f\x4f5c\x6210(&W)",
        L"&\x3042", L"\x3042&\x3044\x3046", L"ab&",
    };
    static const struct { const WCHAR *face; int height, weight; BOOL italic; } fonts[] = {
        {L"MS UI Gothic", -12, FW_NORMAL, FALSE}, {L"MS UI Gothic", -16, FW_BOLD, FALSE},
        {L"Tahoma", -11, FW_NORMAL, FALSE}, {L"Tahoma", -11, FW_BOLD, FALSE}, {L"Tahoma", -13, FW_NORMAL, TRUE},
    };
    BITMAPINFO info = {{sizeof(info.bmiHeader), W, -H, 1, 32, BI_RGB, 0, 0, 0, 0, 0}, {{0, 0, 0, 0}}};
    HDC dc = CreateCompatibleDC(NULL);
    HBITMAP bitmap = CreateDIBSection(dc, &info, DIB_RGB_COLORS, (void **)&bits, NULL, 0);

    if (argc > 1)
    {
        int serial = !lstrcmpiW(argv[1], L"COM1");
        out = CreateFileW(argv[1], GENERIC_WRITE, 0, NULL, serial ? OPEN_EXISTING : CREATE_ALWAYS, 0, NULL);
        if (serial && out != INVALID_HANDLE_VALUE)
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
    emit(L"TEXTUNDERLINE\r\n");
    SelectObject(dc, bitmap);
    SetTextColor(dc, RGB(0, 0, 0));
    SetBkMode(dc, TRANSPARENT);
    for (size_t f = 0; f < ARRAYSIZE(fonts); f++)
        for (size_t t = 0; t < ARRAYSIZE(texts); t++)
            measure(dc, fonts[f].face, fonts[f].height, fonts[f].weight, fonts[f].italic, texts[t],
                    10, W - 10, DT_SINGLELINE | DT_NOCLIP);
    /* Clipping, right alignment and a rectangle that ends inside the character. */
    measure(dc, L"MS UI Gothic", -12, FW_NORMAL, FALSE, L"(&W)", 10, 200 - 10, DT_SINGLELINE | DT_RIGHT);
    measure(dc, L"MS UI Gothic", -12, FW_NORMAL, FALSE, L"(&W)", 10, 200 - 10, DT_SINGLELINE | DT_CENTER);
    measure(dc, L"MS UI Gothic", -12, FW_NORMAL, FALSE, L"&W", 10, 14, DT_SINGLELINE);
    measure(dc, L"MS UI Gothic", -12, FW_NORMAL, FALSE, L"&W", 10, 15, DT_SINGLELINE);
    measure(dc, L"MS UI Gothic", -12, FW_NORMAL, FALSE, L"&W", 10, 16, DT_SINGLELINE);
    measure(dc, L"MS UI Gothic", -12, FW_NORMAL, FALSE, L"&W", 11, 16, DT_SINGLELINE | DT_RIGHT);
    measure(dc, L"MS UI Gothic", -12, FW_NORMAL, FALSE, L"a\t&Wb", 10, 200 - 10, DT_SINGLELINE | DT_EXPANDTABS);
    emit(L"TEXTUNDERLINE complete\r\n");
    DeleteDC(dc);
    DeleteObject(bitmap);
    if (out != INVALID_HANDLE_VALUE) CloseHandle(out);
    return 0;
}
