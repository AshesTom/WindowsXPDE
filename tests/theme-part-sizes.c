/* theme-part-sizes: what public uxtheme calls return for some visual style classes.
 *   theme-part-sizes-test OUT CLASS...
 * Per defined part/state: part sizes, content/extent rects of a 100x30 box,
 * and the properties defined at the class, part or state. */
#include <windows.h>
#include <uxtheme.h>
#include <vssym32.h>
#include <stdio.h>

static FILE *out;

static void value(HTHEME theme, int part, int state, int prop)
{
    COLORREF color; int i; BOOL b; MARGINS m; POINT pt; RECT rc; LOGFONTW lf; WCHAR name[MAX_PATH];
    if (SUCCEEDED(GetThemeColor(theme, part, state, prop, &color))) fprintf(out, " color %06lx", color);
    else if (SUCCEEDED(GetThemeMargins(theme, NULL, part, state, prop, NULL, &m)) && (m.cxLeftWidth || m.cxRightWidth || m.cyTopHeight || m.cyBottomHeight))
        fprintf(out, " margins %d,%d,%d,%d", m.cxLeftWidth, m.cxRightWidth, m.cyTopHeight, m.cyBottomHeight);
    else if (SUCCEEDED(GetThemeInt(theme, part, state, prop, &i))) fprintf(out, " int %d", i);
    else if (SUCCEEDED(GetThemeEnumValue(theme, part, state, prop, &i))) fprintf(out, " enum %d", i);
    else if (SUCCEEDED(GetThemeBool(theme, part, state, prop, &b))) fprintf(out, " bool %d", b);
    else if (SUCCEEDED(GetThemePosition(theme, part, state, prop, &pt))) fprintf(out, " position %ld,%ld", pt.x, pt.y);
    else if (SUCCEEDED(GetThemeRect(theme, part, state, prop, &rc))) fprintf(out, " rect %ld,%ld,%ld,%ld", rc.left, rc.top, rc.right, rc.bottom);
    else if (SUCCEEDED(GetThemeFont(theme, NULL, part, state, prop, &lf))) fprintf(out, " font %ld %ld %ls", lf.lfHeight, lf.lfWeight, lf.lfFaceName);
    else if (SUCCEEDED(GetThemeFilename(theme, part, state, prop, name, MAX_PATH))) fprintf(out, " file %ls", name);
}

int wmain(int argc, WCHAR **argv)
{
    HWND hwnd = CreateWindowW(L"STATIC", L"", WS_POPUP, 0, 0, 10, 10, NULL, NULL, NULL, NULL);
    HDC dc = GetDC(hwnd);
    int a, part, state, prop;

    if (argc < 2 || !(out = _wfopen(argv[1], L"w"))) return 1;
    for (a = 2; a < argc; a++)
    {
        HTHEME theme = OpenThemeData(hwnd, argv[a]);
        fprintf(out, "== %ls %s\n", argv[a], theme ? "" : "(none)");
        if (!theme) continue;
        for (part = 0; part <= 40; part++)
            for (state = 0; state <= 8; state++)
            {
                SIZE min = {0}, true_size = {0};
                RECT box = {0, 0, 100, 30}, content = {0}, extent = {0};
                if ((part || state) && !IsThemePartDefined(theme, part, state)) continue;
                if (!part && state) continue;
                GetThemePartSize(theme, dc, part, state, NULL, TS_MIN, &min);
                GetThemePartSize(theme, dc, part, state, NULL, TS_TRUE, &true_size);
                GetThemeBackgroundContentRect(theme, dc, part, state, &box, &content);
                GetThemeBackgroundExtent(theme, dc, part, state, &box, &extent);
                fprintf(out, "part %d state %d min %ldx%ld true %ldx%ld content %ld,%ld,%ld,%ld extent %ld,%ld,%ld,%ld\n", part, state,
                        min.cx, min.cy, true_size.cx, true_size.cy, content.left, content.top, content.right, content.bottom,
                        extent.left, extent.top, extent.right, extent.bottom);
                for (prop = 1; prop <= 10000; prop++)
                {
                    PROPERTYORIGIN origin;
                    if (FAILED(GetThemePropertyOrigin(theme, part, state, prop, &origin))) continue;
                    if (origin == PO_GLOBAL || origin == PO_NOTFOUND) continue;
                    if (origin == PO_CLASS && part) continue;
                    fprintf(out, "  prop %d origin %d", prop, origin);
                    value(theme, part, state, prop);
                    fprintf(out, "\n");
                }
            }
        CloseThemeData(theme);
    }
    fclose(out);
    return 0;
}
