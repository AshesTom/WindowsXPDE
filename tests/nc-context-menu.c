/* DefWindowProc must generate WM_CONTEXTMENU after a right click in an area
 * that the window itself reports as HTCAPTION, even without WS_CAPTION. */
#include <windows.h>
#include <stdio.h>

static LRESULT hit;
static int context_menus;

static LRESULT CALLBACK proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
    if (msg == WM_NCHITTEST) return hit;
    if (msg == WM_CONTEXTMENU)
    {
        context_menus++;
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wparam, lparam);
}

static int run(HWND hwnd, LRESULT code)
{
    POINT pt;
    MSG msg;
    hit = code;
    context_menus = 0;
    while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) DispatchMessageW(&msg);
    GetCursorPos(&pt);
    ScreenToClient(hwnd, &pt);
    PostMessageW(hwnd, WM_RBUTTONUP, 0, MAKELPARAM(pt.x, pt.y));
    SendMessageW(hwnd, WM_NCRBUTTONDOWN, code, 0);
    while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) DispatchMessageW(&msg);
    printf("hit=%ld context=%d\n", (long)code, context_menus);
    return context_menus;
}

int wmain(void)
{
    WNDCLASSW wc = {0};
    HWND hwnd;
    int failures = 0;

    wc.lpfnWndProc = proc;
    wc.hInstance = GetModuleHandleW(NULL);
    wc.lpszClassName = L"NcContextMenu";
    if (!RegisterClassW(&wc)) return 2;
    hwnd = CreateWindowExW(WS_EX_TOOLWINDOW, wc.lpszClassName, L"", WS_POPUP | WS_VISIBLE,
                           0, 0, 200, 30, NULL, NULL, wc.hInstance, NULL);
    if (!hwnd) return 2;
    failures += run(hwnd, HTCAPTION) != 1;
    /* The unconsumed client WM_RBUTTONUP produces the only menu here. */
    failures += run(hwnd, HTCLIENT) != 1;
    DestroyWindow(hwnd);
    puts(failures ? "FAIL" : "PASS");
    return failures != 0;
}
