/* Exercise external-clock painting through public window messages. */
#ifndef UNICODE
#define UNICODE
#endif
#include <windows.h>
#include <uxtheme.h>
#include <vssym32.h>
#include <stdio.h>

static HTHEME panel_theme;
static int timers, clicks, native_paints, paint_updates, changes, destroyed;
static BOOL destroy_on_paint;
static const COLORREF marker = RGB(250, 0, 250);

static LRESULT CALLBACK parent_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    if (msg == WM_PRINTCLIENT)
    {
        RECT rect;
        GetClientRect(hwnd, &rect);
        DrawThemeBackground(panel_theme, (HDC)wp, 1, 0, &rect, NULL);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

static LRESULT CALLBACK clock_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    if (msg == WM_TIMER) { ++timers; SetWindowTextW(hwnd, L"12:35"); return 0; }
    if (msg == WM_LBUTTONUP) { ++clicks; return 0; }
    if (msg == WM_THEMECHANGED) { ++changes; return 0; }
    if (msg == WM_PAINT)
    {
        PAINTSTRUCT paint;
        ++paint_updates;
        if (destroy_on_paint) { DestroyWindow(hwnd); return 0; }
        SetWindowTextW(hwnd,L"12:36");
        BeginPaint(hwnd,&paint); EndPaint(hwnd,&paint);
        return 0;
    }
    if (msg == WM_NCDESTROY && !GetPropW(hwnd, L"Win4WineClockTheme")) ++destroyed;
    if (msg == WM_PRINTCLIENT)
    {
        RECT rect;
        HBRUSH brush = CreateSolidBrush(marker);
        GetClientRect(hwnd, &rect); FillRect((HDC)wp, &rect, brush);
        DeleteObject(brush); ++native_paints; return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

int wmain(void)
{
    WNDCLASSW cls = {0};
    HWND parent, clock;
    HTHEME theme;
    HDC screen, actual, expected;
    HBITMAP a, e, old_a, old_e;
    RECT all = {0,0,100,50}, rect = {0,0,70,30}, panel = {-20,0,90,30};
    HBRUSH brush = CreateSolidBrush(marker);
    POINT origin;
    COLORREF text_color = RGB(12,34,56);
    int x, y, failed = 0, paints, different = 0;
    if (!IsThemeActive()) return 2;
    theme = OpenThemeData(NULL,L"TrayNotify::Clock");
    panel_theme = OpenThemeData(NULL,L"TrayNotifyHoriz::TrayNotify");
    if (!theme || !panel_theme) return 3;
    cls.hInstance = GetModuleHandleW(NULL); cls.lpfnWndProc = parent_proc;
    cls.lpszClassName = L"TrayNotifyWnd"; if (!RegisterClassW(&cls)) return 4;
    cls.lpfnWndProc = clock_proc; cls.lpszClassName = L"TrayClockWClass";
    if (!RegisterClassW(&cls)) return 4;
    parent = CreateWindowW(L"TrayNotifyWnd",L"",WS_POPUP,0,0,110,30,NULL,NULL,cls.hInstance,NULL);
    screen = GetDC(NULL); actual = CreateCompatibleDC(screen); expected = CreateCompatibleDC(screen);
    a = CreateCompatibleBitmap(screen,100,50); e = CreateCompatibleBitmap(screen,100,50);
    old_a = SelectObject(actual,a); old_e = SelectObject(expected,e); ReleaseDC(NULL,screen);

    SetEnvironmentVariableW(L"WIN4WINE_EXTERNAL_SHELL",NULL);
    clock = CreateWindowW(L"TrayClockWClass",L"12:34",WS_CHILD,20,0,70,30,parent,NULL,cls.hInstance,NULL);
    SetWindowTheme(clock,L"TrayNotify",NULL);
    SendMessageW(clock,WM_PRINTCLIENT,(WPARAM)actual,PRF_CLIENT);
    if (GetPixel(actual,10,10)!=marker || native_paints!=1) failed=1;
    DestroyWindow(clock);

    SetEnvironmentVariableW(L"WIN4WINE_EXTERNAL_SHELL",L"1");
    clock = CreateWindowW(L"TrayClockWClass",L"12:34",WS_CHILD,20,0,70,30,parent,NULL,cls.hInstance,NULL);
    SetWindowTheme(clock,L"TrayNotify",NULL);
    SendMessageW(clock,WM_TIMER,1,0); SendMessageW(clock,WM_LBUTTONUP,0,0);
    SendMessageW(clock,WM_THEMECHANGED,0,0);
    SendMessageW(clock,WM_PAINT,0,0);
    FillRect(actual,&all,brush); FillRect(expected,&all,brush);
    SetViewportOrgEx(actual,13,9,NULL); SetViewportOrgEx(expected,13,9,NULL);
    SetTextColor(actual,text_color); SetBkMode(actual,OPAQUE);
    paints=native_paints;
    SendMessageW(clock,WM_PRINTCLIENT,(WPARAM)actual,PRF_CLIENT);
    IntersectClipRect(expected,0,0,70,30);
    DrawThemeBackground(panel_theme,expected,1,0,&panel,NULL);
    DrawThemeText(theme,expected,CLP_TIME,CLS_NORMAL,L"12:36",-1,
                  DT_CENTER|DT_VCENTER|DT_SINGLELINE|DT_NOPREFIX,0,&rect);
    for(y=0;y<30;y++) for(x=0;x<70;x++)
        if(GetPixel(actual,x,y)!=GetPixel(expected,x,y)) { failed=1; ++different; }
    GetViewportOrgEx(actual,&origin);
    if(origin.x!=13 || origin.y!=9 || GetTextColor(actual)!=text_color ||
       GetBkMode(actual)!=OPAQUE || native_paints!=paints || timers!=1 || clicks!=1 || paint_updates!=1 || changes<3 ||
       GetPixel(actual,-1,0)!=marker || GetPixel(actual,70,0)!=marker) failed=1;

    SetWindowTheme(clock,NULL,NULL);
    SendMessageW(clock,WM_PRINTCLIENT,(WPARAM)actual,PRF_CLIENT);
    if(GetPixel(actual,10,10)!=marker || native_paints!=paints+1) failed=1;
    SetWindowTheme(clock,L"TrayNotify",NULL);
    SendMessageW(clock,WM_PRINTCLIENT,(WPARAM)actual,PRF_CLIENT);
    if(GetPixel(actual,0,0)==marker || native_paints!=paints+1) failed=1;
    DestroyWindow(clock); if(destroyed!=2) failed=1;
    clock = CreateWindowW(L"TrayClockWClass",L"12:34",WS_CHILD,20,0,70,30,parent,NULL,cls.hInstance,NULL);
    SetWindowTheme(clock,L"TrayNotify",NULL);
    destroy_on_paint=TRUE;
    SendMessageW(clock,WM_PAINT,0,0);
    if (IsWindow(clock) || destroyed!=3) failed=1;
    if (failed) printf("different=%d origin=%ld,%ld paints=%d timers=%d clicks=%d changes=%d destroyed=%d\n",
                       different,origin.x,origin.y,native_paints,timers,clicks,changes,destroyed);
    DestroyWindow(parent);
    SelectObject(actual,old_a); SelectObject(expected,old_e);
    DeleteObject(a);DeleteObject(e);DeleteObject(brush);DeleteDC(actual);DeleteDC(expected);
    CloseThemeData(theme);CloseThemeData(panel_theme);
    puts(failed ? "FAIL clock theme, message delegation or DC preservation" :
         "PASS clock theme/translated print/DC preservation/native paint time update/timer and clicks/theme toggle/cleanup");
    return failed;
}
