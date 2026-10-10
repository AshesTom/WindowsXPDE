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
        printf("event=%u hwnd=%08lx class=%ls tick=%lu\n", (unsigned)wp, (unsigned long)lp, cls, GetTickCount());
        fflush(stdout);
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}
int wmain(void)
{
    WNDCLASSW cls = {0};
    HWND hwnd, parent;
    MSG msg;
    ULONGLONG until = GetTickCount64() + 20000;
    cls.lpfnWndProc = subscriber;
    cls.hInstance = GetModuleHandleW(NULL);
    cls.lpszClassName = L"Win4WineTestSubscriber";
    if (!RegisterClassW(&cls)) return 1;
    shell_message = RegisterWindowMessageW(L"SHELLHOOK");
    parent = CreateWindowW(cls.lpszClassName, L"hook parent", 0, 0, 0, 1, 1, NULL, NULL, cls.hInstance, NULL);
    hwnd = CreateWindowW(cls.lpszClassName, L"hook subscriber", WS_CHILD, 0, 0, 1, 1, parent, NULL, cls.hInstance, NULL);
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
    DestroyWindow(parent);
    return 0;
}
