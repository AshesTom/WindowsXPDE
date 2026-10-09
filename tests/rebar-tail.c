/* Public control geometry regression for the external task band's unused tail. */
#ifndef UNICODE
#define UNICODE
#endif
#include <windows.h>
#include <commctrl.h>
#include <uxtheme.h>
#include <stdio.h>
static HWND child;
static BOOL vertical;
static UINT band_style,header;
static RECT all,child_rect;
static LRESULT CALLBACK procedure(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp)
{
    switch(msg)
    {
    case RB_GETBANDCOUNT:return 1;
    case RB_GETBANDINFOW:
        ((REBARBANDINFOW *)lp)->fStyle=band_style;
        ((REBARBANDINFOW *)lp)->cxHeader=header;
        ((REBARBANDINFOW *)lp)->hwndChild=child;return TRUE;
    case RB_GETRECT:*(RECT *)lp=all;return TRUE;
    case WM_PRINTCLIENT:
        FillRect((HDC)wp,&all,GetSysColorBrush(COLOR_BTNFACE));
        {HBRUSH brush=CreateSolidBrush(RGB(202,17,93));FillRect((HDC)wp,&child_rect,brush);DeleteObject(brush);}
        return 17;
    case WM_LBUTTONDOWN:return 42;
    }
    return DefWindowProcW(hwnd,msg,wp,lp);
}
int wmain(void)
{
    WNDCLASSW wc={0};HWND tray,rebar;HDC screen,actual,expected;HBITMAP a,b,oa,ob;
    int i,x,y,failed=0;RECT tail,clip_before,clip_after;POINT origin;
    if(!IsThemeActive())return 2;
    wc.hInstance=GetModuleHandleW(NULL);wc.lpfnWndProc=DefWindowProcW;wc.lpszClassName=L"Shell_TrayWnd";
    if(!RegisterClassW(&wc))return 2;
    wc.lpszClassName=L"MSTaskSwWClass";if(!RegisterClassW(&wc))return 2;
    wc.lpszClassName=L"ReBarWindow32";wc.lpfnWndProc=procedure;if(!RegisterClassW(&wc))return 2;
    screen=GetDC(NULL);actual=CreateCompatibleDC(screen);expected=CreateCompatibleDC(screen);
    a=CreateCompatibleBitmap(screen,210,210);b=CreateCompatibleBitmap(screen,210,210);ReleaseDC(NULL,screen);
    oa=SelectObject(actual,a);ob=SelectObject(expected,b);
    for(i=0;i<8;i++)
    {
        int different=0;BOOL skip=i==4||i==5||i==6,clipped=i==7;
        vertical=i==2||i==3;header=(i==1||i==3)?9:0;
        band_style=RBBS_HIDETITLE|(header?RBBS_GRIPPERALWAYS:RBBS_NOGRIPPER);
        if(i==4)band_style|=RBBS_USECHEVRON;
        if(i==5)band_style|=RBBS_CHILDEDGE;
        if(i==6)band_style|=RBBS_HIDDEN;
        SetRect(&all,0,0,vertical?30:200,vertical?200:30);
        child_rect=all;if(vertical){child_rect.top=header;child_rect.bottom-=4;}else{child_rect.left=header;child_rect.right-=4;}
        tray=CreateWindowW(L"Shell_TrayWnd",L"",WS_POPUP,0,GetSystemMetrics(SM_CYSCREEN)-30,all.right,all.bottom,NULL,NULL,wc.hInstance,NULL);
        rebar=CreateWindowW(L"ReBarWindow32",L"",WS_CHILD|(vertical?CCS_VERT:0),0,0,all.right,all.bottom,tray,NULL,wc.hInstance,NULL);
        child=CreateWindowW(L"MSTaskSwWClass",L"",WS_CHILD,child_rect.left,child_rect.top,child_rect.right-child_rect.left,child_rect.bottom-child_rect.top,rebar,NULL,wc.hInstance,NULL);
        if(!tray||!rebar||!child||FAILED(SetWindowTheme(rebar,L"Taskbar",NULL)))return 2;
        SetViewportOrgEx(actual,5,3,NULL);SetViewportOrgEx(expected,5,3,NULL);
        SelectClipRgn(actual,NULL);SelectClipRgn(expected,NULL);
        /* The reference consists solely of original native paint plus the
         * parent-background API clipped to the public child trailing edge. */
        procedure(rebar,WM_PRINTCLIENT,(WPARAM)expected,PRF_CLIENT);
        if(clipped){IntersectClipRect(actual,198,15,200,30);IntersectClipRect(expected,198,15,200,30);}
        tail=all;if(vertical)tail.top=child_rect.bottom;else tail.left=child_rect.right;
        if(!skip)DrawThemeParentBackground(rebar,expected,&tail);
        GetClipBox(actual,&clip_before);
        if(SendMessageW(rebar,WM_PRINTCLIENT,(WPARAM)actual,PRF_CLIENT)!=17)failed=1;
        GetClipBox(actual,&clip_after);
        if(!EqualRect(&clip_before,&clip_after))failed=1;
        SelectClipRgn(actual,NULL);SelectClipRgn(expected,NULL);
        for(y=vertical?header:0;y<all.bottom;y++)for(x=vertical?0:header;x<all.right;x++)
        {
            /* In the clipped case original WM_PRINTCLIENT must also respect
             * the supplied clip, so only its affected area is compared. */
            if(clipped && (x<198||y<15))continue;
            if(GetPixel(actual,x,y)!=GetPixel(expected,x,y))different++;
        }
        GetViewportOrgEx(actual,&origin);if(origin.x!=5||origin.y!=3)failed=1;
        if(SendMessageW(rebar,WM_LBUTTONDOWN,0,0)!=42)failed=1;
        printf("case=%d vertical=%d header=%u skipped=%d clipped=%d mismatches=%d\n",i,vertical,header,skip,clipped,different);
        if(different)failed=1;
        DestroyWindow(tray);
    }
    SelectObject(actual,oa);SelectObject(expected,ob);DeleteObject(a);DeleteObject(b);DeleteDC(actual);DeleteDC(expected);
    puts(failed?"FAIL: external task ReBar tail":"PASS: locked/unlocked/vertical tails, child preservation, exclusions, clipped translated DC and input");return failed;
}
