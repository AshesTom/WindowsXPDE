/* trackbar-size: trackbar channel and thumb rectangles for a few sizes and styles.
 *   trackbar-size-test OUT */
#include <windows.h>
#include <commctrl.h>
#include <stdio.h>
static void one(FILE *out, HWND parent, DWORD style, int w, int h)
{
    HWND tb = CreateWindowW(TRACKBAR_CLASSW, L"", WS_CHILD | WS_VISIBLE | style, 10, 10, w, h, parent, NULL, NULL, NULL);
    RECT ch, th;
    SendMessageW(tb, TBM_GETCHANNELRECT, 0, (LPARAM)&ch);
    SendMessageW(tb, TBM_GETTHUMBRECT, 0, (LPARAM)&th);
    fprintf(out, "style %08lx %dx%d channel %ld,%ld,%ld,%ld thumb %ld,%ld,%ld,%ld thumblen %ld\n", style, w, h,
            ch.left, ch.top, ch.right, ch.bottom, th.left, th.top, th.right, th.bottom,
            (long)SendMessageW(tb, TBM_GETTHUMBLENGTH, 0, 0));
    DestroyWindow(tb);
}
int wmain(int argc, WCHAR **argv)
{
    FILE *out = argc > 1 ? _wfopen(argv[1], L"w") : stdout;
    INITCOMMONCONTROLSEX icc = { sizeof(icc), ICC_BAR_CLASSES };
    HWND parent;
    InitCommonControlsEx(&icc);
    parent = CreateWindowW(L"STATIC", L"p", WS_OVERLAPPEDWINDOW | WS_VISIBLE, 0, 0, 400, 300, NULL, NULL, NULL, NULL);
    {
        int h;
        for (h = 6; h <= 24; h++) one(out, parent, TBS_AUTOTICKS, 200, h);
        for (h = 16; h <= 26; h += 2) one(out, parent, TBS_AUTOTICKS | TBS_BOTH, 200, h);
    }
    fclose(out);
    return 0;
}
