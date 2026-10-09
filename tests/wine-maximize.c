/* Public first-map maximize/restore regression, with an active bridge WM. */
#ifndef UNICODE
#define UNICODE
#endif
#define _UNICODE
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
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
static BOOL bounded;
static LRESULT CALLBACK fixture_proc(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp)
{
    if (msg==WM_GETMINMAXINFO && bounded)
    {
        MINMAXINFO *info=(MINMAXINFO *)lp;
        info->ptMaxPosition.x=40; info->ptMaxPosition.y=50;
        info->ptMaxSize.x=600; info->ptMaxSize.y=450;
        return 0;
    }
    return DefWindowProcW(hwnd,msg,wp,lp);
}
static BOOL check_max(HWND hwnd,int width,int height,const char *label)
{
    WINDOWINFO info={.cbSize=sizeof(info)}; WINDOWPLACEMENT placement={.length=sizeof(placement)}; RECT r; BOOL result;
    GetWindowInfo(hwnd,&info); GetWindowRect(hwnd,&r); GetWindowPlacement(hwnd,&placement);
    result=IsZoomed(hwnd) && r.left==-(LONG)info.cxWindowBorders && r.top==-(LONG)info.cyWindowBorders
        && r.right==width+(LONG)info.cxWindowBorders && r.bottom==height+(LONG)info.cyWindowBorders
        && placement.rcNormalPosition.left==80 && placement.rcNormalPosition.top==90
        && placement.rcNormalPosition.right==580 && placement.rcNormalPosition.bottom==490;
    printf("%s max rect=%ld,%ld..%ld,%ld borders=%u,%u zoomed=%d expected-workarea=%d,%d\n",
           label,r.left,r.top,r.right,r.bottom,info.cxWindowBorders,info.cyWindowBorders,IsZoomed(hwnd),width,height);
    printf("owner-process normal=%ld,%ld..%ld,%ld\n",placement.rcNormalPosition.left,placement.rcNormalPosition.top,
           placement.rcNormalPosition.right,placement.rcNormalPosition.bottom);
    return result;
}
static BOOL exercise(HWND owner,int width,int height,const char *label)
{
    RECT r; BOOL result=TRUE,pass;
    HWND hwnd=CreateWindowExW(owner?WS_EX_APPWINDOW:0,L"Win4WineMaximizeFixture",L"Initial maximized fixture",
        WS_OVERLAPPEDWINDOW,80,90,500,400,owner,NULL,GetModuleHandleW(NULL),NULL);
    if (!hwnd) return FALSE;
    ShowWindow(hwnd,SW_SHOWMAXIMIZED); pump(2200);
    pass=check_max(hwnd,width,height,label); result &= pass;
    printf("%s %s first-map maximize fills workarea with measured borders\n",pass?"PASS":"FAIL",label);
    SendMessageW(hwnd,WM_SYSCOMMAND,SC_RESTORE,0); pump(1200);
    GetWindowRect(hwnd,&r);
    pass=!IsZoomed(hwnd) && r.left==80 && r.top==90 && r.right==580 && r.bottom==490; result &= pass;
    printf("%s %s restore preserves original normal rectangle (%ld,%ld..%ld,%ld)\n",pass?"PASS":"FAIL",label,r.left,r.top,r.right,r.bottom);
    for (unsigned i=0;i<3;i++)
    {
        SendMessageW(hwnd,WM_SYSCOMMAND,SC_MAXIMIZE,0); pump(30);
        SendMessageW(hwnd,WM_SYSCOMMAND,SC_RESTORE,0); pump(30);
    }
    SendMessageW(hwnd,WM_SYSCOMMAND,SC_MAXIMIZE,0); pump(2200);
    pass=check_max(hwnd,width,height,label); result &= pass;
    printf("%s %s rapid maximize/restore/re-max settles correctly\n",pass?"PASS":"FAIL",label);
    SendMessageW(hwnd,WM_SYSCOMMAND,SC_RESTORE,0); pump(1200);
    GetWindowRect(hwnd,&r);
    pass=!IsZoomed(hwnd) && r.left==80 && r.top==90 && r.right==580 && r.bottom==490; result &= pass;
    printf("%s %s rapid cycle retains normal rectangle (%ld,%ld..%ld,%ld)\n",pass?"PASS":"FAIL",label,r.left,r.top,r.right,r.bottom);
    DestroyWindow(hwnd);
    return result;
}
int wmain(int argc,WCHAR **argv)
{
    WNDCLASSW wc={0}; HWND owner; BOOL result; int width,height;
    if (argc!=3) return 2;
    width=_wtoi(argv[1]); height=_wtoi(argv[2]);
    if (width<=0 || height<=0) return 2;
    wc.lpfnWndProc=fixture_proc; wc.hInstance=GetModuleHandleW(NULL); wc.lpszClassName=L"Win4WineMaximizeFixture";
    if (!RegisterClassW(&wc)) return 2;
    result=exercise(NULL,width,height,"normal Wine");
    owner=CreateWindowW(wc.lpszClassName,L"Owned app parent",WS_OVERLAPPEDWINDOW|WS_VISIBLE,
        20,20,320,200,NULL,NULL,wc.hInstance,NULL);
    if (!owner) return 2;
    result &= exercise(owner,width,height,"owned APPWINDOW Wine");
    DestroyWindow(owner);
    bounded=TRUE;
    owner=CreateWindowW(wc.lpszClassName,L"Custom bounded max",WS_OVERLAPPEDWINDOW,
        80,90,500,400,NULL,NULL,wc.hInstance,NULL);
    if (!owner) return 2;
    ShowWindow(owner,SW_MAXIMIZE); pump(1500);
    {
        RECT r; BOOL pass;
        GetWindowRect(owner,&r);
        pass=IsZoomed(owner) && r.left==40 && r.top==50 && r.right==640 && r.bottom==500;
        printf("%s custom WM_GETMINMAXINFO bounds respected (%ld,%ld..%ld,%ld)\n",pass?"PASS":"FAIL",r.left,r.top,r.right,r.bottom);
        result &= pass;
        SendMessageW(owner,WM_SYSCOMMAND,SC_RESTORE,0); pump(1200); GetWindowRect(owner,&r);
        pass=!IsZoomed(owner) && r.left==80 && r.top==90 && r.right==580 && r.bottom==490;
        printf("%s custom bounded max restores original normal rectangle\n",pass?"PASS":"FAIL"); result &= pass;
    }
    DestroyWindow(owner);
    return result?0:1;
}
