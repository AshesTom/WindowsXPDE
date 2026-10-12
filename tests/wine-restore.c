/* Minimize a Wine window and restore it from Win32, with an active bridge WM:
 * prints its HWND, then "minimized" and "restored" as it goes. */
#ifndef UNICODE
#define UNICODE
#endif
#define _UNICODE
#include <windows.h>
#include <stdio.h>
static void pump(DWORD duration)
{
    DWORD start=GetTickCount(); MSG msg;
    while (GetTickCount()-start<duration)
    {
        while (PeekMessageW(&msg,NULL,0,0,PM_REMOVE))
        { TranslateMessage(&msg); DispatchMessageW(&msg); }
        Sleep(10);
    }
}
int wmain(void)
{
    WNDCLASSW wc={.lpfnWndProc=DefWindowProcW,.hInstance=GetModuleHandleW(NULL),.lpszClassName=L"Win4WineRestore",
                  .hbrBackground=(HBRUSH)(COLOR_WINDOW+1)};
    HWND hwnd;
    RegisterClassW(&wc);
    hwnd=CreateWindowW(L"Win4WineRestore",L"restore test",WS_OVERLAPPEDWINDOW,100,100,300,200,NULL,NULL,wc.hInstance,NULL);
    ShowWindow(hwnd,SW_SHOWNORMAL);
    printf("hwnd %p\n",hwnd); fflush(stdout);
    pump(1500);
    ShowWindow(hwnd,SW_MINIMIZE);
    pump(1500);
    printf("minimized %d\n",IsIconic(hwnd)); fflush(stdout);
    ShowWindow(hwnd,SW_RESTORE);
    pump(2000);
    printf("restored %d\n",!IsIconic(hwnd)); fflush(stdout);
    /* Keep the window until the checker closes standard input. */
    while (getchar()!=EOF) ;
    DestroyWindow(hwnd);
    return 0;
}
