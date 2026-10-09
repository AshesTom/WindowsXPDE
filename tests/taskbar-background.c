/* Compare taskbar child background to the user's active theme's public part. */
#ifndef UNICODE
#define UNICODE
#endif
#include <windows.h>
#include <uxtheme.h>
#include <vssym32.h>
#include <stdio.h>

int wmain(void)
{
    WNDCLASSW cls = {0};
    HWND tray, child;
    HTHEME theme;
    HDC screen, actual, expected;
    HBITMAP a, e, old_a, old_e;
    RECT parent = {-120,-2,680,38}, clip = {10,4,290,26}, all = {0,0,350,40};
    POINT origin;
    HBRUSH marker = CreateSolidBrush(RGB(250,0,250));
    int x, y, failed = 0;
    if (!IsThemeActive()) return 2;
    cls.lpfnWndProc = DefWindowProcW;
    cls.hInstance = GetModuleHandleW(NULL);
    cls.lpszClassName = L"Shell_TrayWnd";
    if (!RegisterClassW(&cls)) return 3;
    tray = CreateWindowW(cls.lpszClassName, L"theme test", WS_POPUP,
                        0, GetSystemMetrics(SM_CYSCREEN)-40, 800, 40, NULL, NULL, cls.hInstance, NULL);
    child = CreateWindowW(L"Static", L"child", WS_CHILD, 120, 2, 300, 30, tray, NULL, cls.hInstance, NULL);
    theme = OpenThemeData(NULL, L"Taskbar");
    if (!tray || !child || !theme) return 4;
    screen = GetDC(NULL);
    actual = CreateCompatibleDC(screen); expected = CreateCompatibleDC(screen);
    a = CreateCompatibleBitmap(screen,350,40); e = CreateCompatibleBitmap(screen,350,40);
    old_a = SelectObject(actual,a); old_e = SelectObject(expected,e);
    ReleaseDC(NULL,screen);
    SetViewportOrgEx(actual,5,3,NULL); SetViewportOrgEx(expected,5,3,NULL);
    FillRect(actual,&all,marker); FillRect(expected,&all,marker);
    if (FAILED(DrawThemeBackground(theme,expected,TBP_BACKGROUNDBOTTOM,0,&parent,NULL)) ||
        FAILED(DrawThemeParentBackground(child,actual,&clip))) return 5;
    for (y=4;y<26;y+=3) for (x=10;x<290;x+=13)
        if (GetPixel(actual,x,y)!=GetPixel(expected,x,y)) failed=1;
    if (GetPixel(actual,9,4)!=RGB(250,0,250) || GetPixel(actual,10,26)!=RGB(250,0,250)) failed=1;
    GetViewportOrgEx(actual,&origin);
    if (origin.x!=5 || origin.y!=3) failed=1;
    if (failed) fputs("FAIL: taskbar descendant theme alignment, clip or DC state\n",stderr);
    else puts("PASS: taskbar descendant background matches active theme, clipping, translated caller DC");
    SelectObject(actual,old_a); SelectObject(expected,old_e);
    DeleteObject(a);DeleteObject(e);DeleteObject(marker);DeleteDC(actual);DeleteDC(expected);
    CloseThemeData(theme);DestroyWindow(tray);
    return failed;
}
