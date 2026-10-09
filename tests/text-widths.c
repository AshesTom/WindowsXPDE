/* Compare text advances of Japanese UI faces with public GDI metrics.
 * Usage: text-widths [COM1|FILE]. Prints selected face, extents and widths. */
#include <windows.h>
#include <stdio.h>
#include <string.h>

static HANDLE out = INVALID_HANDLE_VALUE;

static void emit(const WCHAR *text)
{
    char utf8[2048];
    DWORD written;
    int length = WideCharToMultiByte(CP_UTF8, 0, text, -1, utf8, sizeof(utf8), NULL, NULL);
    if (length <= 1) return;
    if (out == INVALID_HANDLE_VALUE) fputs(utf8, stdout);
    else WriteFile(out, utf8, length - 1, &written, NULL);
}

int wmain(int argc, WCHAR **argv)
{
    static const WCHAR *faces[] = {L"MS UI Gothic", L"MS Gothic", L"MS PGothic", L"\xff2d\xff33 \xff30\x30b4\x30b7\x30c3\x30af"};
    static const WCHAR *texts[] = {L"\x30de\x30a4 \x30b3\x30f3\x30d4\x30e5\x30fc\x30bf", L"Internet Explorer",
                                   L"\x30b9\x30bf\x30fc\x30c8", L"\x30ed\x30b0\x30aa\x30d5"};
    static const int heights[] = {-12, -13, -16};
    WCHAR line[1024], face[LF_FACESIZE];
    HDC dc = CreateCompatibleDC(NULL);
    ICONMETRICSW icon = {.cbSize = sizeof(icon)};

    if (argc > 1)
    {
        out = CreateFileW(argv[1], GENERIC_WRITE, 0, NULL,
                          wcsncmp(argv[1], L"COM", 3) ? CREATE_ALWAYS : OPEN_EXISTING, 0, NULL);
        if (out != INVALID_HANDLE_VALUE && !wcsncmp(argv[1], L"COM", 3))
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
    SystemParametersInfoW(SPI_GETICONMETRICS, sizeof(icon), &icon, 0);
    swprintf(line, ARRAYSIZE(line), L"TEXTWIDTHS iconfont=%ls height=%ld weight=%ld spacing=%d,%d wrap=%d\r\n",
             icon.lfFont.lfFaceName, icon.lfFont.lfHeight, icon.lfFont.lfWeight,
             icon.iHorzSpacing, icon.iVertSpacing, icon.iTitleWrap);
    emit(line);
    for (unsigned f = 0; f < ARRAYSIZE(faces); f++)
        for (unsigned h = 0; h < ARRAYSIZE(heights); h++)
        {
            HFONT font = CreateFontW(heights[h], 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                                     OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
                                     DEFAULT_PITCH, faces[f]);
            HGDIOBJ old = SelectObject(dc, font);
            TEXTMETRICW tm;
            GetTextFaceW(dc, ARRAYSIZE(face), face);
            GetTextMetricsW(dc, &tm);
            swprintf(line, ARRAYSIZE(line), L"face request=%ls height=%d selected=%ls ascent=%ld descent=%ld avg=%ld max=%ld\r\n",
                     faces[f], heights[h], face, tm.tmAscent, tm.tmDescent, tm.tmAveCharWidth, tm.tmMaxCharWidth);
            emit(line);
            for (unsigned t = 0; t < ARRAYSIZE(texts); t++)
            {
                SIZE size;
                int widths[64], n = (int)wcslen(texts[t]), pos = swprintf(line, ARRAYSIZE(line), L"  text=%ls", texts[t]);
                GetTextExtentPoint32W(dc, texts[t], n, &size);
                pos += swprintf(line + pos, ARRAYSIZE(line) - pos, L" extent=%ld widths=", size.cx);
                for (int i = 0; i < n; i++)
                {
                    GetCharWidth32W(dc, texts[t][i], texts[t][i], &widths[i]);
                    pos += swprintf(line + pos, ARRAYSIZE(line) - pos, L"%d,", widths[i]);
                }
                swprintf(line + pos, ARRAYSIZE(line) - pos, L"\r\n");
                emit(line);
            }
            SelectObject(dc, old);
            DeleteObject(font);
        }
    emit(L"TEXTWIDTHS complete\r\n");
    if (out != INVALID_HANDLE_VALUE) CloseHandle(out);
    return 0;
}
