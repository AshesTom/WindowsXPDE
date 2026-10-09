/* Public SWP_NOREDRAW screen preservation: a visible window must retain the
 * underlying pixels until its owner draws. Matches an unchanged XP reference. */
#ifndef UNICODE
#define UNICODE
#endif
#include <windows.h>
#include <stdio.h>
static int paints, failures;
static const COLORREF underlay=RGB(23,109,53), painted=RGB(153,51,119), explicit_color=RGB(37,71,173);
static LRESULT CALLBACK procedure(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp)
{
    if(msg==WM_NCCREATE)SetWindowLongPtrW(hwnd,GWLP_USERDATA,(LONG_PTR)((CREATESTRUCTW*)lp)->lpCreateParams);
    if(msg==WM_ERASEBKGND){RECT r;HBRUSH b=CreateSolidBrush((COLORREF)GetWindowLongPtrW(hwnd,GWLP_USERDATA));GetClientRect(hwnd,&r);FillRect((HDC)wp,&r,b);DeleteObject(b);return 1;}
    if(msg==WM_PAINT){PAINTSTRUCT ps;BeginPaint(hwnd,&ps);SendMessageW(hwnd,WM_ERASEBKGND,(WPARAM)ps.hdc,0);EndPaint(hwnd,&ps);paints++;return 0;}
    return DefWindowProcW(hwnd,msg,wp,lp);
}
static COLORREF pixel(HWND hwnd,BOOL screen)
{
    RECT r;HDC dc=GetDC(screen?NULL:hwnd);COLORREF result;GetWindowRect(hwnd,&r);
    result=GetPixel(dc,screen?r.left+30:30,screen?r.top+30:30);ReleaseDC(screen?NULL:hwnd,dc);return result;
}
static void check(const char *label,BOOL condition)
{
    printf("%s %s\n",condition?"PASS":"FAIL",label);if(!condition)failures++;
}
static void restore(HWND popup,HWND back)
{
    ShowWindow(popup,SW_HIDE);RedrawWindow(back,NULL,NULL,RDW_INVALIDATE|RDW_ERASE|RDW_UPDATENOW);
}
int wmain(void)
{
    WNDCLASSW wc={0};HWND back,popup,layered;RECT rect={0,0,200,120};HDC dc;HBRUSH brush;
    wc.lpfnWndProc=procedure;wc.hInstance=GetModuleHandleW(NULL);wc.lpszClassName=L"Win4WineNoRedrawTest";
    if(!RegisterClassW(&wc))return 2;
    back=CreateWindowExW(WS_EX_TOPMOST,wc.lpszClassName,L"NOREDRAW public test",WS_POPUP|WS_VISIBLE,0,0,500,300,NULL,NULL,wc.hInstance,(void*)(ULONG_PTR)underlay);
    popup=CreateWindowExW(WS_EX_TOPMOST,wc.lpszClassName,L"",WS_POPUP,100,100,200,120,back,NULL,wc.hInstance,(void*)(ULONG_PTR)painted);
    if(!back||!popup)return 2;
    UpdateWindow(back);
    for(int cycle=0;cycle<2;cycle++)
    {
        paints=0;
        check("show succeeds",SetWindowPos(popup,HWND_TOPMOST,0,0,0,0,SWP_NOSIZE|SWP_NOMOVE|SWP_NOACTIVATE|SWP_SHOWWINDOW|SWP_NOREDRAW));
        check("visible without owner paint",IsWindowVisible(popup)&&!paints);
        check("initial/reshow underlying screen retained",pixel(popup,TRUE)==underlay);
        Sleep(100);
        check("NOREDRAW keeps underlying screen without WM_PAINT",pixel(popup,TRUE)==underlay&&!paints);
        dc=GetDC(popup);brush=CreateSolidBrush(explicit_color);FillRect(dc,&rect,brush);DeleteObject(brush);
        check("explicit owner DC receives pixels",GetPixel(dc,30,30)==explicit_color);ReleaseDC(popup,dc);
        RedrawWindow(popup,NULL,NULL,RDW_INVALIDATE|RDW_ERASE|RDW_UPDATENOW);
        check("normal paint replaces retained screen",pixel(popup,TRUE)==painted&&paints==1);
        restore(popup,back);
    }
    /* No-redraw state must end on a later ordinary show, with its normal
     * paint/erase behavior and buffered DC contents intact. */
    paints=0;ShowWindow(popup,SW_SHOWNOACTIVATE);UpdateWindow(popup);
    check("ordinary show after NOREDRAW paints",pixel(popup,TRUE)==painted&&paints>0);
    restore(popup,back);
    /* Root reads must clip an off-screen popup instead of asking XGetImage
     * for negative coordinates or copying beyond the screen. */
    SetWindowPos(popup,NULL,-10,20,0,0,SWP_NOSIZE|SWP_NOZORDER|SWP_NOACTIVATE|SWP_NOREDRAW);
    SetWindowPos(popup,HWND_TOPMOST,0,0,0,0,SWP_NOSIZE|SWP_NOMOVE|SWP_NOACTIVATE|SWP_SHOWWINDOW|SWP_NOREDRAW);
    check("partially off-screen NOREDRAW retains visible underlay",pixel(popup,TRUE)==underlay);
    restore(popup,back);
    layered=CreateWindowExW(WS_EX_TOPMOST|WS_EX_LAYERED,wc.lpszClassName,L"",WS_POPUP,100,100,200,120,back,NULL,wc.hInstance,(void*)(ULONG_PTR)painted);
    if(!layered)return 2;
    SetLayeredWindowAttributes(layered,0,255,LWA_ALPHA);
    ShowWindow(layered,SW_SHOWNOACTIVATE);RedrawWindow(layered,NULL,NULL,RDW_INVALIDATE|RDW_ERASE|RDW_UPDATENOW);
    check("excluded layered popup still paints",pixel(layered,FALSE)==painted);
    DestroyWindow(layered);DestroyWindow(popup);DestroyWindow(back);
    printf("NOREDRAW failures=%d (immediate GdiFlush screen presentation is separate)\n",failures);return !!failures;
}
