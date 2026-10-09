/* Public theme-part and input-preservation regression; no embedded theme data. */
#ifndef UNICODE
#define UNICODE
#endif
#include <windows.h>
#include <commctrl.h>
#include <uxtheme.h>
#include <vssym32.h>
#include <stdio.h>
static int clicks, paints, destroy_paint;
static LRESULT CALLBACK rebar_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    RECT all={0,0,200,30};
    switch(msg)
    {
    case RB_GETBANDCOUNT:return 1;
    case RB_GETBANDINFOW:
        ((REBARBANDINFOW *)lp)->fStyle=RBBS_GRIPPERALWAYS|RBBS_HIDETITLE;
        ((REBARBANDINFOW *)lp)->cxHeader=9;return 1;
    case RB_GETRECT:*(RECT *)lp=all;return 1;
    case WM_LBUTTONDOWN:++clicks;return 42;
    case WM_PRINTCLIENT:++paints;FillRect((HDC)wp,&all,GetSysColorBrush(COLOR_BTNFACE));
        if(destroy_paint)DestroyWindow(hwnd);
        return 17;
    }
    return DefWindowProcW(hwnd,msg,wp,lp);
}
int wmain(void)
{
    WNDCLASSW cls={0};HWND tray,rebar;HDC screen,actual,expected;HBITMAP a,e,oa,oe;
    HTHEME theme;SIZE size;RECT header={0,0,9,30},grip,all={0,0,200,30};POINT origin;
    int x,y,failed=0;
    if(!IsThemeActive())return 2;
    cls.hInstance=GetModuleHandleW(NULL);cls.lpfnWndProc=DefWindowProcW;cls.lpszClassName=L"Shell_TrayWnd";
    if(!RegisterClassW(&cls))return 3;
    cls.lpfnWndProc=rebar_proc;cls.lpszClassName=L"ReBarWindow32";
    if(!RegisterClassW(&cls))return 4;
    tray=CreateWindowW(L"Shell_TrayWnd",L"test",WS_POPUP,0,GetSystemMetrics(SM_CYSCREEN)-30,200,30,NULL,NULL,cls.hInstance,NULL);
    rebar=CreateWindowW(L"ReBarWindow32",L"test",WS_CHILD,0,0,200,30,tray,NULL,cls.hInstance,NULL);
    if(!tray||!rebar||FAILED(SetWindowTheme(rebar,L"Taskbar",NULL)))return 5;
    theme=OpenThemeData(NULL,L"Taskbar::Rebar");
    if(!theme||FAILED(GetThemePartSize(theme,NULL,RP_GRIPPER,0,NULL,TS_TRUE,&size)))return 6;
    screen=GetDC(NULL);actual=CreateCompatibleDC(screen);expected=CreateCompatibleDC(screen);
    a=CreateCompatibleBitmap(screen,210,40);e=CreateCompatibleBitmap(screen,210,40);ReleaseDC(NULL,screen);
    oa=SelectObject(actual,a);oe=SelectObject(expected,e);
    SetViewportOrgEx(actual,5,3,NULL);SetViewportOrgEx(expected,5,3,NULL);
    FillRect(expected,&all,GetSysColorBrush(COLOR_BTNFACE));DrawThemeParentBackground(rebar,expected,&header);
    /* Actual XP public band measurement: anchored at header origin, with
     * theme margins/tiling spanning the complete long axis. */
    grip.left=0;grip.top=0;grip.right=size.cx;grip.bottom=30;
    DrawThemeBackground(theme,expected,RP_GRIPPER,0,&grip,&header);
    if(SendMessageW(rebar,WM_PRINTCLIENT,(WPARAM)actual,PRF_CLIENT)!=17)failed=1;
    for(y=0;y<30;++y)for(x=0;x<200;++x)if(GetPixel(actual,x,y)!=GetPixel(expected,x,y))failed=1;
    GetViewportOrgEx(actual,&origin);if(origin.x!=5||origin.y!=3)failed=1;
    if(SendMessageW(rebar,WM_LBUTTONDOWN,0,MAKELPARAM(4,15))!=42||clicks!=1||paints!=1)failed=1;
    SetWindowTheme(rebar,NULL,NULL);SendMessageW(rebar,WM_PRINTCLIENT,(WPARAM)actual,PRF_CLIENT);
    if(GetPixel(actual,4,15)!=GetSysColor(COLOR_BTNFACE))failed=1;
    SetWindowTheme(rebar,L"Taskbar",NULL);destroy_paint=1;
    if(SendMessageW(rebar,WM_PRINTCLIENT,(WPARAM)actual,PRF_CLIENT)!=17 || IsWindow(rebar))failed=1;
    puts(failed?"FAIL: external ReBar theme, translated DC or native dispatch":"PASS: external ReBar matches real theme gripper/background, translated DC, native paint/input and disable");
    CloseThemeData(theme);DestroyWindow(tray);SelectObject(actual,oa);SelectObject(expected,oe);
    DeleteObject(a);DeleteObject(e);DeleteDC(actual);DeleteDC(expected);return failed;
}
