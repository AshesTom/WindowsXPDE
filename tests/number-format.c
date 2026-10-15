/* number-format: GetNumberFormatW for a few values and formats (zero, negative zero).
 *   number-format-test OUT */
#include <windows.h>
#include <stdio.h>

int wmain(int argc, WCHAR **argv)
{
    static const WCHAR *values[] = { L"0", L"0.0", L"0.5", L"3", L"1234", L"-0", L"00" };
    FILE *out;
    unsigned int i, lz;
    if (argc < 2 || !(out = _wfopen(argv[1], L"w"))) return 1;
    for (i = 0; i < ARRAYSIZE(values); i++)
        for (lz = 0; lz < 2; lz++)
        {
            NUMBERFMTW fmt = { 0, lz, 3, (WCHAR *)L".", (WCHAR *)L",", 1 };
            WCHAR buf[64] = L"?";
            int r = GetNumberFormatW(LOCALE_USER_DEFAULT, 0, values[i], &fmt, buf, 64);
            fwprintf(out, L"%ls lz%u digits0 -> %d '%ls'\n", values[i], lz, r, buf);
            fmt.NumDigits = 2;
            r = GetNumberFormatW(LOCALE_USER_DEFAULT, 0, values[i], &fmt, buf, 64);
            fwprintf(out, L"%ls lz%u digits2 -> %d '%ls'\n", values[i], lz, r, buf);
        }
    fclose(out);
    return 0;
}
