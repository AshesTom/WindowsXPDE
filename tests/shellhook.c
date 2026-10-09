/* Verify the bridge's public shell-hook notification path. */
#ifndef UNICODE
#define UNICODE
#endif
#include <windows.h>
#include <stdio.h>
static UINT shell_message;
static LRESULT CALLBACK subscriber(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    if (msg == shell_message)
    {
        WCHAR cls[256] = {0};
        GetClassNameW((HWND)lp, cls, 256);
        printf("event=%u hwnd=%08lx class=%ls\n", (unsigned)wp, (unsigned long)lp, cls);
        fflush(stdout);
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}
int wmain(void)
{
    WNDCLASSW cls = {0};
    HWND hwnd;
    MSG msg;
    ULONGLONG until = GetTickCount64() + 20000;
    cls.lpfnWndProc = subscriber;
    cls.hInstance = GetModuleHandleW(NULL);
    cls.lpszClassName = L"Win4WineTestSubscriber";
    if (!RegisterClassW(&cls)) return 1;
    shell_message = RegisterWindowMessageW(L"SHELLHOOK");
    hwnd = CreateWindowW(cls.lpszClassName, L"hook subscriber", 0, 0, 0, 1, 1, NULL, NULL, cls.hInstance, NULL);
    if (!RegisterShellHookWindow(hwnd)) return 2;
    puts("subscribed");
    fflush(stdout);
    while (GetTickCount64() < until)
    {
        while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        MsgWaitForMultipleObjects(0, NULL, FALSE, 100, QS_ALLINPUT);
    }
    if (!DeregisterShellHookWindow(hwnd)) return 3;
    DestroyWindow(hwnd);
    return 0;
}
