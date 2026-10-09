/* Print where DrawText's DT_WORDBREAK ends each line, for self-chosen Japanese
 * and mixed strings at several widths. Usage: text-wrap [COM1]
 * A line's end is found by comparing the pixels of its row band with the
 * prefix drawn on one line; only the break offsets are printed. */
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static HANDLE out = INVALID_HANDLE_VALUE;
static HDC dc, line_dc;
static DWORD *bits, *line_bits;
enum { W = 640, H = 200 };

static void emit(const char *text)
{
    DWORD written;
    if (out == INVALID_HANDLE_VALUE) fputs(text, stdout);
    else WriteFile(out, text, (DWORD)strlen(text), &written, NULL);
}

static HDC surface(DWORD **pixels)
{
    BITMAPINFO info = {0};
    HDC result = CreateCompatibleDC(NULL);
    info.bmiHeader.biSize = sizeof(info.bmiHeader);
    info.bmiHeader.biWidth = W;
    info.bmiHeader.biHeight = -H;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    SelectObject(result, CreateDIBSection(result, &info, DIB_RGB_COLORS, (void **)pixels, NULL, 0));
    SetBkMode(result, TRANSPARENT);
    return result;
}

static void clear(DWORD *pixels)
{
    for (int i = 0; i < W * H; i++) pixels[i] = 0xffffff;
}

/* Number of characters DrawText puts on the first line of text at width. */
static int first_line(const WCHAR *text, int length, int width, int height)
{
    RECT rect = {0, 0, width, H};
    clear(bits);
    DrawTextW(dc, text, length, &rect, DT_WORDBREAK | DT_NOPREFIX | DT_LEFT);
    GdiFlush();
    for (int count = 1; count <= length; count++)
    {
        RECT one = {0, 0, W, H};
        BOOL same = TRUE;
        clear(line_bits);
        DrawTextW(line_dc, text, count, &one, DT_SINGLELINE | DT_NOPREFIX | DT_LEFT);
        GdiFlush();
        for (int y = 0; same && y < height; y++)
            for (int x = 0; same && x < W; x++)
                if ((bits[y * W + x] & 0xffffff) != (line_bits[y * W + x] & 0xffffff)) same = FALSE;
        if (same) return count;
    }
    return -1;
}

int main(int argc, char **argv)
{
    static const WCHAR *texts[] = {
        L"\x5b9f\x884c\x3059\x308b\x30d7\x30ed\x30b0\x30e9\x30e0\x540d\x3001\x307e\x305f\x306f\x958b\x304f\x30d5\x30a9\x30eb\x30c0\x3084\x30c9\x30ad\x30e5\x30e1\x30f3\x30c8\x540d\x3001\x30a4\x30f3\x30bf\x30fc\x30cd\x30c3\x30c8 \x30ea\x30bd\x30fc\x30b9\x540d\x3092\x5165\x529b\x3057\x3066\x304f\x3060\x3055\x3044\x3002",
        L"\x300c\x30c6\x30b9\x30c8\x300d\x3068\x300e\x5c0f\x3055\x306a\x30c3\x30e3\x30e5\x30e7\x300f\x3001\x3002\x30fc\x30fc\x30a1\x30a3\x3005\x309d\xff01\xff1f\xff09\xff08",
        L"Windows XP \x306b\x642d\x8f09\x3055\x308c\x305f\x65b0\x3057\x3044\x6a5f\x80fd\x3092\x77e5\x308b\x306b\x306f\x3001\x3053\x3053\x3092\x30af\x30ea\x30c3\x30af\x3057\x3066\x304f\x3060\x3055\x3044\x3002",
        L"abc def\x3042\x3044\x3046(\x3048)\x304a,\x304b.\x304d \x304f",
    };
    char line[1024];
    LOGFONTW lf = {0};
    HFONT font;
    TEXTMETRICW metrics;

    if (argc > 1 && !_strnicmp(argv[1], "COM", 3))
    {
        out = CreateFileA(argv[1], GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
        if (out != INVALID_HANDLE_VALUE)
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
    dc = surface(&bits);
    line_dc = surface(&line_bits);
    lf.lfHeight = -12;
    lf.lfCharSet = SHIFTJIS_CHARSET;
    wcscpy(lf.lfFaceName, L"MS UI Gothic");
    font = CreateFontIndirectW(&lf);
    SelectObject(dc, font);
    SelectObject(line_dc, font);
    GetTextMetricsW(dc, &metrics);
    if (argc > 1 && !strcmp(argv[1], "--widths"))
    {
        for (unsigned t = 0; t < ARRAYSIZE(texts); t++)
        {
            int length = lstrlenW(texts[t]), fit, n, *dx = calloc(length, sizeof(int));
            SIZE size;
            GetTextExtentExPointW(dc, texts[t], length, 100000, &fit, dx, &size);
            n = sprintf(line, "widths t%u:", t);
            for (int i = 0; i < length; i++) n += sprintf(line + n, " %d", dx[i]);
            puts(line);
            free(dx);
        }
        return 0;
    }
    if (!strcmp(argv[argc - 1], "--pairs"))
    {
        /* Single-byte x and y after a double-byte character, the natural
         * break between them: 6 = broken there, 5 = before x. */
        static const char pairs[] = "ab" "a!" "!a" "!)" "a(" "(a" "1." ".1" "a-" "-a" "a/" "/a" "a," ",a" "a&" "&a"
                                    "a$" "$a" "a\"" "\"a" "a'" "'a" "a#" "#a" "a@" "@a" "a+" "+a" "a=" "=a" "a<" "<a"
                                    "a?" "?a" "a%" "%a" "a)" ")a" "a]" "]a" "a[" "[a" "a{" "{a" "a}" "}a" "a:" ":a"
                                    "a;" ";a" "a*" "*a" "a_" "_a" "a~" "~a" "a|" "|a" "a^" "^a" "a`" "`a" "((" "))"
                                    ".." "-1" "1-" "a1" "1a" "11";
        for (unsigned i = 0; i + 1 < sizeof(pairs); i += 2)
        {
            WCHAR text[16] = L"\x304b\x304d\x304f\x3051\x3042";
            SIZE size;
            int count;
            text[5] = pairs[i];
            text[6] = pairs[i + 1];
            lstrcpyW(text + 7, L"\x3055\x3057\x3059");
            GetTextExtentPoint32W(dc, text, 6, &size);
            count = first_line(text, lstrlenW(text), size.cx, metrics.tmHeight);
            sprintf(line, "pair %c%c line=%d\r\n", pairs[i], pairs[i + 1], count);
            emit(line);
        }
        emit("TEXTWRAP complete\r\n");
        return 0;
    }
    if (!strcmp(argv[argc - 1], "--triples"))
    {
        /* pp, p and n with the natural break before n: where does it go? */
        static const WCHAR *triples[] = {
            L"\x3042\x3002\x3001", L"\x3042\x300d\x3002", L"\x300d\x3002\x3001", L"\x3002\x300d\x3001",
            L"\x3042\xff01\xff1f", L"\x3001\x3002\x300d", L"\x3044\x3001\x300d", L"\xff01\xff1f\x3001",
            L"\x3042\x3005\x3002", L"\x3005\x3005\x3002", L"\x30a2\x30fc\x3002", L"\x30fc\x3002\x3001",
            L"\x3042\x3001\x3001", L"\x3001\x3001\x3001", L"\x3042\x3063\x3002", L"\x3063\x3002\x3002",
            L"a\x3001\x3002", L"\x3042!)", L"\x3042),", L"\x300c\x3042\x3002",
        };
        for (unsigned i = 0; i < ARRAYSIZE(triples); i++)
        {
            WCHAR text[32];
            SIZE size;
            int count;
            lstrcpyW(text, L"\x304b\x304d\x304f\x3051");
            lstrcatW(text, triples[i]);
            lstrcatW(text, L"\x3055\x3057\x3059");
            GetTextExtentPoint32W(dc, text, 6, &size);
            count = first_line(text, lstrlenW(text), size.cx, metrics.tmHeight);
            sprintf(line, "triple %04x %04x %04x line=%d\r\n", triples[i][0], triples[i][1], triples[i][2], count);
            emit(line);
        }
        emit("TEXTWRAP complete\r\n");
        return 0;
    }
    if (!strcmp(argv[argc - 1], "--chars"))
    {
        /* For each character c: does a line break before it (start) or after
         * it (end) where it would fall naturally between two hiragana? */
        static const WCHAR chars[] =
            L"\x3001\x3002\xff0c\xff0e\x30fb\xff1a\xff1b\xff1f\xff01\x309b\x309c\x30fd\x30fe\x309d\x309e\x3005\x30fc\x2015\x2010"
            L"\xff5e\x2026\x2025\x2019\x201d\xff09\x3015\xff3d\xff5d\x3009\x300b\x300d\x300f\x3011\xff08\x3014\xff3b\xff5b"
            L"\x3008\x300a\x300c\x300e\x3010\x2018\x201c\x3041\x3043\x3045\x3047\x3049\x3063\x3083\x3085\x3087\x308e"
            L"\x30a1\x30a3\x30a5\x30a7\x30a9\x30c3\x30e3\x30e5\x30e7\x30ee\x30f5\x30f6\xff61\xff62\xff63\xff64\xff65\xff70"
            L"\xff9e\xff9f!%),.:;?]}([{$\\\x00a5\xffe5\xff04\x2032\x2033\x2103\xff05\x00b0\x4e00\x30a2" L"Aa1-/&'\"#@*+=<>";
        WCHAR text[32];
        int base;
        SIZE size;
        for (unsigned i = 0; i < ARRAYSIZE(chars) - 1; i++)
        {
            int n, start, end;
            /* five hiragana, c, five hiragana; width ends just before c */
            for (int k = 0; k < 11; k++) text[k] = 0x3042 + 2 * (k % 5);
            text[5] = chars[i];
            text[11] = 0;
            GetTextExtentPoint32W(dc, text, 5, &size);
            base = size.cx;
            start = first_line(text, 11, base, metrics.tmHeight);
            GetTextExtentPoint32W(dc, text, 6, &size);
            end = first_line(text, 11, size.cx, metrics.tmHeight);
            n = sprintf(line, "char %04x start=%d end=%d\r\n", chars[i], start, end);
            (void)n;
            emit(line);
        }
        emit("TEXTWRAP complete\r\n");
        return 0;
    }
    emit("TEXTWRAP\r\n");
    for (unsigned t = 0; t < ARRAYSIZE(texts); t++)
        for (int width = 30; width <= 330; width += 11)
        {
            const WCHAR *text = texts[t];
            int length = lstrlenW(text), n, offset = 0, lines = 0;
            n = sprintf(line, "t%u w%d:", t, width);
            while (offset < length && lines++ < 40)
            {
                int count = first_line(text + offset, length - offset, width, metrics.tmHeight);
                if (count <= 0) { n += sprintf(line + n, " ?"); break; }
                offset += count;
                /* DrawText drops the space that ends a line. */
                if (offset < length && text[offset] == ' ') offset++;
                n += sprintf(line + n, " %d", offset);
            }
            sprintf(line + n, "\r\n");
            emit(line);
        }
    emit("TEXTWRAP complete\r\n");
    return 0;
}
