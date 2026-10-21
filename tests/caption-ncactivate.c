/* A caption follows WM_NCACTIVATE, also for a window that is not the foreground window
 * (patch 0069; bridge-wm's frames of Linux windows are drawn active this way).
 * Usage: caption-ncactivate. Run with WIN4WINE_EXTERNAL_SHELL, Luna or classic. */
#include <windows.h>
#include <stdio.h>

static COLORREF caption_pixel(HWND hwnd)
{
    HDC dc = GetWindowDC(hwnd);
    /* Inside the caption, left of the title text, right of the icon. */
    COLORREF color = GetPixel(dc, GetSystemMetrics(SM_CXFRAME) + 30, GetSystemMetrics(SM_CYFRAME) + 4);
    ReleaseDC(hwnd, dc);
    return color;
}

static void pump(void)
{
    MSG msg;
    DWORD end = GetTickCount() + 300;
    while (GetTickCount() < end)
    {
        while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) DispatchMessageW(&msg);
        Sleep(10);
    }
}

int main(void)
{
    HWND a, b;
    COLORREF active, inactive, forced, cleared;
    int failures = 0;
    a = CreateWindowW(L"STATIC", L"frame", WS_OVERLAPPEDWINDOW | WS_VISIBLE, 40, 40, 300, 160, NULL, NULL, NULL, NULL);
    b = CreateWindowW(L"STATIC", L"foreground", WS_OVERLAPPEDWINDOW | WS_VISIBLE, 400, 40, 300, 160, NULL, NULL, NULL, NULL);
    SetForegroundWindow(b);
    pump();
    if (GetForegroundWindow() != b) { printf("SKIP: could not make the second window the foreground\n"); return 2; }
    active = caption_pixel(b);
    inactive = caption_pixel(a);
    /* Every theme draws inactive captions differently; a window that only lost the
     * foreground still looking active is the fault this checks (Luna before). */
    if (active == inactive) { printf("FAIL: the window that is not the foreground looks active (%06lx)\n", inactive); return 1; }
    SendMessageW(a, WM_NCACTIVATE, TRUE, 0);
    pump();
    forced = caption_pixel(a);
    SendMessageW(a, WM_NCACTIVATE, FALSE, 0);
    pump();
    cleared = caption_pixel(a);
    if (forced != active) { printf("FAIL: WM_NCACTIVATE(TRUE) caption %06lx, active %06lx\n", forced, active); failures++; }
    if (cleared != inactive) { printf("FAIL: WM_NCACTIVATE(FALSE) caption %06lx, inactive %06lx\n", cleared, inactive); failures++; }
    if (GetForegroundWindow() != b) { printf("FAIL: the foreground window changed\n"); failures++; }
    if (!failures) printf("PASS: the caption follows WM_NCACTIVATE without the foreground (active %06lx, inactive %06lx)\n",
                          active, inactive);
    DestroyWindow(a);
    DestroyWindow(b);
    return failures != 0;
}
