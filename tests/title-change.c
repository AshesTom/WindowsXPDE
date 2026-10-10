/* A Win32 task that renames itself, for the bridge's shell notifications. */
#ifndef UNICODE
#define UNICODE
#endif
#include <windows.h>
#include <stdio.h>

static void pump(DWORD milliseconds)
{
    ULONGLONG until = GetTickCount64() + milliseconds;
    MSG msg;
    while (GetTickCount64() < until)
    {
        while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) DispatchMessageW(&msg);
        MsgWaitForMultipleObjects(0, NULL, FALSE, 10, QS_ALLINPUT);
    }
}

int wmain(void)
{
    WNDCLASSW cls = {0};
    HWND hwnd;
    cls.lpfnWndProc = DefWindowProcW;
    cls.hInstance = GetModuleHandleW(NULL);
    cls.lpszClassName = L"Win4WineTitleProbe";
    if (!RegisterClassW(&cls)) return 1;
    hwnd = CreateWindowW(cls.lpszClassName, L"title probe", WS_OVERLAPPEDWINDOW | WS_VISIBLE,
                         100, 100, 300, 200, NULL, NULL, cls.hInstance, NULL);
    if (!hwnd) return 2;
    printf("hwnd=%08lx\n", (unsigned long)(UINT_PTR)hwnd);
    fflush(stdout);
    pump(1500);
    printf("renamed tick=%lu\n", GetTickCount());
    fflush(stdout);
    SetWindowTextW(hwnd, L"title probe renamed");
    pump(2000);
    DestroyWindow(hwnd);
    return 0;
}
