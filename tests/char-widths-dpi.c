/* Public probe: advance widths of MS UI Gothic's ASCII characters at several pixel heights.
 * Usage: char-widths-dpi.exe OUT. Numbers only. */
#ifndef UNICODE
#define UNICODE
#endif
#include <windows.h>
#include <stdio.h>

int main(int argc, char **argv)
{
    static const int heights[] = { 12, 13, 15, 16, 18, 20, 24 };
    FILE *out = fopen(argc > 1 ? argv[1] : "com1", "wb");
    HDC dc = GetDC(NULL);
    unsigned i, weight;

    if (!out) return 1;
    for (weight = 400; weight <= 700; weight += 300)
        for (i = 0; i < sizeof(heights) / sizeof(heights[0]); i++)
        {
            HFONT font = CreateFontW(-heights[i], 0, 0, 0, weight, FALSE, FALSE, FALSE, SHIFTJIS_CHARSET, 0, 0, 0, 0,
                                     L"MS UI Gothic");
            HGDIOBJ old = SelectObject(dc, font);
            INT widths[95];
            int c;
            GetCharWidth32W(dc, 32, 126, widths);
            fprintf(out, "h%d w%u:", heights[i], weight);
            for (c = 0; c < 95; c++) fprintf(out, " %d", widths[c]);
            fprintf(out, "\n");
            DeleteObject(SelectObject(dc, old));
        }
    fclose(out);
    return 0;
}
