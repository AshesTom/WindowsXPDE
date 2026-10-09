/* Print the black pixels of classic caption DrawFrameControl glyphs.
 * Usage: frame-control [COM1|FILE] | --verify [COM1|FILE]
 * --verify checks the XP rule measured with this program: the Marlett em is
 * the square side minus 4, drawn 2px inside the square (3px when pushed).
 * The same self-authored program runs on Wine and on the reference XP
 * machine; only public USER/GDI APIs are used and only masks are written. */
#include <windows.h>
#include <stdio.h>
#include <string.h>

static HANDLE out = INVALID_HANDLE_VALUE;

static DWORD rgb(COLORREF color)
{
    return (GetRValue(color) << 16) | (GetGValue(color) << 8) | GetBValue(color);
}

static void emit(const char *text)
{
    DWORD written;
    if (out == INVALID_HANDLE_VALUE) fputs(text, stdout);
    else WriteFile(out, text, (DWORD)strlen(text), &written, NULL);
}

static void open_output(const WCHAR *target)
{
    out = CreateFileW(target, GENERIC_WRITE, 0, NULL,
                      wcsncmp(target, L"COM", 3) ? CREATE_ALWAYS : OPEN_EXISTING, 0, NULL);
    if (out != INVALID_HANDLE_VALUE && !wcsncmp(target, L"COM", 3))
    {
        DCB dcb = {.DCBlength = sizeof(dcb)};
        COMMTIMEOUTS timeouts = {.WriteTotalTimeoutConstant = 2000};
        GetCommState(out, &dcb);
        dcb.BaudRate = CBR_115200;
        dcb.ByteSize = 8;
        dcb.Parity = NOPARITY;
        dcb.StopBits = ONESTOPBIT;
        dcb.fOutxCtsFlow = dcb.fOutxDsrFlow = FALSE;
        dcb.fRtsControl = RTS_CONTROL_ENABLE;
        dcb.fDtrControl = DTR_CONTROL_ENABLE;
        SetCommState(out, &dcb);
        SetCommTimeouts(out, &timeouts);
    }
}

static int verify(void)
{
    static const UINT kinds[] = {DFCS_CAPTIONMIN, DFCS_CAPTIONMAX, DFCS_CAPTIONRESTORE,
                                 DFCS_CAPTIONCLOSE, DFCS_CAPTIONHELP};
    static const WCHAR glyphs[] = {'0', '1', '2', 'r', 's'};
    BITMAPINFO info = {0};
    DWORD *a, *b;
    HDC da = CreateCompatibleDC(NULL), db = CreateCompatibleDC(NULL);
    HBITMAP ba, bb;
    int failures = 0;

    info.bmiHeader.biSize = sizeof(info.bmiHeader);
    info.bmiHeader.biWidth = 64;
    info.bmiHeader.biHeight = -64;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    ba = CreateDIBSection(da, &info, DIB_RGB_COLORS, (void **)&a, NULL, 0);
    bb = CreateDIBSection(db, &info, DIB_RGB_COLORS, (void **)&b, NULL, 0);
    SelectObject(da, ba);
    SelectObject(db, bb);
    SetBkMode(db, TRANSPARENT);
    for (int height = 10; height <= 30; height++)
        for (int width = height; width <= height + 3; width++)
            for (int k = 0; k < 5; k++)
                for (int pushed = 0; pushed < 2; pushed++)
                {
                    RECT r = {0, 0, width, height};
                    int side = min(width, height), left = (width - side) / 2 + 2 + pushed;
                    DWORD text = rgb(GetSysColor(COLOR_BTNTEXT));
                    HFONT font = CreateFontW(-(side - 4), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                             SYMBOL_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                             DEFAULT_QUALITY, FIXED_PITCH | FF_DONTCARE, L"Marlett");
                    HGDIOBJ old = SelectObject(db, font);
                    int bad = 0;
                    memset(a, 0xee, 64 * 64 * 4);
                    memset(b, 0xee, 64 * 64 * 4);
                    DrawFrameControl(da, &r, DFC_CAPTION, kinds[k] | (pushed ? DFCS_PUSHED : 0));
                    SetTextColor(db, GetSysColor(COLOR_BTNTEXT));
                    TextOutW(db, left, 2 + pushed, &glyphs[k], 1);
                    GdiFlush();
                    for (int y = 0; y < height; y++)
                        for (int x = 0; x < width; x++)
                            bad += ((a[y * 64 + x] & 0xffffff) == text) != ((b[y * 64 + x] & 0xffffff) == text);
                    if (bad)
                    {
                        char line[96];
                        snprintf(line, sizeof(line), "mismatch %dx%d glyph=%c pushed=%d pixels=%d\r\n",
                                 width, height, (char)glyphs[k], pushed, bad);
                        emit(line);
                        failures++;
                    }
                    SelectObject(db, old);
                    DeleteObject(font);
                }
    emit(failures ? "FAIL\r\n" : "PASS\r\n");
    return failures != 0;
}

int wmain(int argc, WCHAR **argv)
{
    static const struct { UINT state; const char *name; } kinds[] = {
        {DFCS_CAPTIONMIN, "min"}, {DFCS_CAPTIONMAX, "max"}, {DFCS_CAPTIONRESTORE, "restore"},
        {DFCS_CAPTIONCLOSE, "close"}, {DFCS_CAPTIONHELP, "help"},
    };
    static const struct { UINT state; const char *name; } modes[] = {
        {0, ""}, {DFCS_PUSHED, "-pushed"}, {DFCS_INACTIVE, "-inactive"},
    };
    static const SIZE sizes[] = {{16, 14}, {12, 10}, {14, 12}, {18, 16}, {20, 18}, {24, 22}, {28, 26}, {32, 30}};
    char line[512];
    BITMAPINFO info = {0};
    DWORD *bits;
    HDC dc;
    HBITMAP bitmap;

    if (argc > 1 && !wcscmp(argv[1], L"--verify"))
    {
        int result;
        if (argc > 2) open_output(argv[2]);
        result = verify();
        if (out != INVALID_HANDLE_VALUE) CloseHandle(out);
        return result;
    }
    if (argc > 1) open_output(argv[1]);
    dc = CreateCompatibleDC(NULL);
    info.bmiHeader.biSize = sizeof(info.bmiHeader);
    info.bmiHeader.biWidth = 64;
    info.bmiHeader.biHeight = -64;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    bitmap = CreateDIBSection(dc, &info, DIB_RGB_COLORS, (void **)&bits, NULL, 0);
    if (!dc || !bitmap) return 2;
    SelectObject(dc, bitmap);
    snprintf(line, sizeof(line), "FRAMECONTROL btntext=%06lx\r\n", (unsigned long)GetSysColor(COLOR_BTNTEXT));
    emit(line);
    for (unsigned m = 0; m < ARRAYSIZE(modes); m++)
    for (unsigned s = 0; s < ARRAYSIZE(sizes); s++)
        for (unsigned k = 0; k < ARRAYSIZE(kinds); k++)
        {
            RECT r = {0, 0, sizes[s].cx, sizes[s].cy};
            DWORD text = rgb(GetSysColor(COLOR_BTNTEXT)), shadow = rgb(GetSysColor(COLOR_BTNSHADOW));
            DWORD light = rgb(GetSysColor(COLOR_BTNHIGHLIGHT));
            if (m && s) continue;
            memset(bits, 0xee, 64 * 64 * 4);
            DrawFrameControl(dc, &r, DFC_CAPTION, kinds[k].state | modes[m].state);
            GdiFlush();
            snprintf(line, sizeof(line), "glyph %s%s %ldx%ld\r\n", kinds[k].name, modes[m].name,
                     sizes[s].cx, sizes[s].cy);
            emit(line);
            for (int y = 0; y < sizes[s].cy; y++)
            {
                int x;
                for (x = 0; x < sizes[s].cx; x++)
                {
                    DWORD v = bits[y * 64 + x] & 0xffffff;
                    line[x] = v == text ? '#' : !m ? '.' : v == shadow ? 'g' : v == light ? 'w' : '.';
                }
                line[x++] = '\r';
                line[x++] = '\n';
                line[x] = 0;
                emit(line);
            }
        }
    emit("FRAMECONTROL complete\r\n");
    if (out != INVALID_HANDLE_VALUE) CloseHandle(out);
    return 0;
}
