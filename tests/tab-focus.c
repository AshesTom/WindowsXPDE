/* tab-focus: does a tab control take the focus when clicked?
 *   tab-focus-test OUT */
#include <windows.h>
#include <commctrl.h>
#include <stdio.h>
#include <string.h>
static void one(FILE *out, HWND parent, HWND button, DWORD style)
{
    HWND tab = CreateWindowExW(0, WC_TABCONTROLW, L"", WS_CHILD | WS_VISIBLE | style, 10, 40, 300, 70, parent, NULL, NULL, NULL);
    TCITEMW ti;
    RECT rc;
    MSG msg;
    memset(&ti, 0, sizeof(ti));
    ti.mask = TCIF_TEXT;
    ti.pszText = (WCHAR *)L"one"; SendMessageW(tab, TCM_INSERTITEMW, 0, (LPARAM)&ti);
    ti.pszText = (WCHAR *)L"two"; SendMessageW(tab, TCM_INSERTITEMW, 1, (LPARAM)&ti);
    SetFocus(button);
    SendMessageW(tab, TCM_GETITEMRECT, 1, (LPARAM)&rc);
    SendMessageW(tab, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(rc.left + 5, rc.top + 5));
    fprintf(out, "style %08lx after down: focus tab %d button %d sel %ld\n", style, GetFocus() == tab, GetFocus() == button,
            (long)SendMessageW(tab, TCM_GETCURSEL, 0, 0));
    SendMessageW(tab, WM_LBUTTONUP, 0, MAKELPARAM(rc.left + 5, rc.top + 5));
    while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) DispatchMessageW(&msg);
    fprintf(out, "  after up: focus tab %d button %d sel %ld\n", GetFocus() == tab, GetFocus() == button,
            (long)SendMessageW(tab, TCM_GETCURSEL, 0, 0));
    DestroyWindow(tab);
}
int wmain(int argc, WCHAR **argv)
{
    FILE *out = argc > 1 ? _wfopen(argv[1], L"w") : stdout;
    INITCOMMONCONTROLSEX icc = { sizeof(icc), ICC_TAB_CLASSES };
    HWND parent, button;
    InitCommonControlsEx(&icc);
    parent = CreateWindowW(L"STATIC", L"p", WS_OVERLAPPEDWINDOW | WS_VISIBLE, 0, 0, 400, 300, NULL, NULL, NULL, NULL);
    button = CreateWindowW(L"BUTTON", L"b", WS_CHILD | WS_VISIBLE | WS_TABSTOP, 10, 10, 50, 20, parent, NULL, NULL, NULL);
    SetForegroundWindow(parent);
    one(out, parent, button, 0);
    one(out, parent, button, TCS_FOCUSONBUTTONDOWN);
    one(out, parent, button, TCS_FOCUSNEVER);
    fclose(out);
    return 0;
}
