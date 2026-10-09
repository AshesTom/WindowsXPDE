/* Compare actual non-client scrollbar arrow states with public theme drawing. */
#ifndef UNICODE
#define UNICODE
#endif
#include <windows.h>
#include <uxtheme.h>
#include <vssym32.h>
#include <stdio.h>

static int compare_arrow(HWND hwnd, HTHEME theme, LONG object, int state, BOOL second)
{
    SCROLLBARINFO info = {0};
    RECT window, arrow;
    HDC source, actual, expected;
    HBITMAP bitmap_a, bitmap_e, old_a, old_e;
    int x, y, width, height, differences = 0;
    info.cbSize = sizeof(info);
    if (!GetScrollBarInfo(hwnd, object, &info)) return 1;
    GetWindowRect(hwnd, &window);
    /* Use public arrow metrics: Wine 10 GetScrollBarInfo reports thumb size
     * in dxyLineButton. That unrelated API defect must not skew this capture. */
    arrow = info.rcScrollBar;
    OffsetRect(&arrow, -window.left, -window.top);
    if (object == OBJID_VSCROLL)
    {
        if (second) arrow.top = arrow.bottom - GetSystemMetrics(object == OBJID_VSCROLL ? SM_CYVSCROLL : SM_CXHSCROLL);
        else arrow.bottom = arrow.top + GetSystemMetrics(object == OBJID_VSCROLL ? SM_CYVSCROLL : SM_CXHSCROLL);
    }
    else
    {
        if (second) arrow.left = arrow.right - GetSystemMetrics(object == OBJID_VSCROLL ? SM_CYVSCROLL : SM_CXHSCROLL);
        else arrow.right = arrow.left + GetSystemMetrics(object == OBJID_VSCROLL ? SM_CYVSCROLL : SM_CXHSCROLL);
    }
    width = arrow.right - arrow.left;
    height = arrow.bottom - arrow.top;
    source = GetWindowDC(hwnd);
    actual = CreateCompatibleDC(source); expected = CreateCompatibleDC(source);
    bitmap_a = CreateCompatibleBitmap(source, width, height);
    bitmap_e = CreateCompatibleBitmap(source, width, height);
    old_a = SelectObject(actual, bitmap_a); old_e = SelectObject(expected, bitmap_e);
    BitBlt(actual, 0, 0, width, height, source, arrow.left, arrow.top, SRCCOPY);
    SetRect(&arrow, 0, 0, width, height);
    if (FAILED(DrawThemeBackground(theme, expected, SBP_ARROWBTN, state, &arrow, NULL)))
        differences = 1;
    for (y = 0; y < height; y++) for (x = 0; x < width; x++)
        differences += GetPixel(actual, x, y) != GetPixel(expected, x, y);
    printf("bar=%s arrow=%s state=%d different-pixels=%d\n",
           object == OBJID_VSCROLL ? "vertical" : "horizontal", second ? "second" : "first", state, differences);
    SelectObject(actual, old_a); SelectObject(expected, old_e);
    DeleteObject(bitmap_a); DeleteObject(bitmap_e);
    DeleteDC(actual); DeleteDC(expected); ReleaseDC(hwnd, source);
    return differences != 0;
}

int wmain(void)
{
    WNDCLASSW wc = {0};
    HWND hwnd;
    HTHEME theme;
    SCROLLINFO info = {sizeof(info), SIF_RANGE | SIF_PAGE | SIF_POS, 0, 99, 20, 40, 0};
    MSG message;
    int failed = 0, bar, mode;
    const UINT flags[] = {ESB_ENABLE_BOTH, ESB_DISABLE_LTUP, ESB_DISABLE_RTDN, ESB_DISABLE_BOTH};
    if (!IsThemeActive()) return 2;
    wc.lpfnWndProc = DefWindowProcW;
    wc.hInstance = GetModuleHandleW(NULL);
    wc.lpszClassName = L"Scrollbar theme probe";
    wc.hbrBackground = GetSysColorBrush(COLOR_WINDOW);
    if (!RegisterClassW(&wc)) return 2;
    hwnd = CreateWindowW(wc.lpszClassName, L"Scrollbar state probe", WS_OVERLAPPEDWINDOW | WS_VISIBLE | WS_VSCROLL | WS_HSCROLL,
                         100, 100, 420, 280, NULL, NULL, wc.hInstance, NULL);
    theme = OpenThemeData(NULL, L"Scrollbar");
    if (!hwnd || !theme) return 2;
    SetCursorPos(900, 700);
    for (bar = SB_HORZ; bar <= SB_VERT; bar++) SetScrollInfo(hwnd, bar, &info, TRUE);
    for (mode = 0; mode < 5; mode++)
    {
        EnableWindow(hwnd, mode != 4);
        for (bar = SB_HORZ; bar <= SB_VERT; bar++)
            EnableScrollBar(hwnd, bar, flags[mode % 4]);
        RedrawWindow(hwnd, NULL, NULL, RDW_INVALIDATE | RDW_FRAME | RDW_UPDATENOW);
        while (PeekMessageW(&message, NULL, 0, 0, PM_REMOVE)) DispatchMessageW(&message);
        GdiFlush();
        printf("enabled-window=%d enable-flags=%u\n", mode != 4, flags[mode % 4]);
        failed |= compare_arrow(hwnd, theme, OBJID_VSCROLL,
            mode == 4 || (flags[mode % 4] & ESB_DISABLE_LTUP) ? ABS_UPDISABLED : ABS_UPNORMAL, FALSE);
        failed |= compare_arrow(hwnd, theme, OBJID_VSCROLL,
            mode == 4 || (flags[mode % 4] & ESB_DISABLE_RTDN) ? ABS_DOWNDISABLED : ABS_DOWNNORMAL, TRUE);
        failed |= compare_arrow(hwnd, theme, OBJID_HSCROLL,
            mode == 4 || (flags[mode % 4] & ESB_DISABLE_LTUP) ? ABS_LEFTDISABLED : ABS_LEFTNORMAL, FALSE);
        failed |= compare_arrow(hwnd, theme, OBJID_HSCROLL,
            mode == 4 || (flags[mode % 4] & ESB_DISABLE_RTDN) ? ABS_RIGHTDISABLED : ABS_RIGHTNORMAL, TRUE);
    }
    DestroyWindow(hwnd); CloseThemeData(theme);
    if (!failed) puts("PASS: themed non-client arrows preserve independent disabled states");
    return failed;
}
