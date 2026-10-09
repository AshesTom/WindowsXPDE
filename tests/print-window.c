/* Public WM_PRINT regression. No native DLL or private shell data is used. */
#ifndef UNICODE
#define UNICODE
#endif
#include <windows.h>
#include <stdio.h>

static int failed;
static const COLORREF untouched = RGB(1,2,3);
static LRESULT CALLBACK paint_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    if (msg == WM_DRAWITEM)
    {
        DRAWITEMSTRUCT *item = (DRAWITEMSTRUCT *)lp;
        HBRUSH brush = CreateSolidBrush(RGB(230,220,10));
        FillRect(item->hDC, &item->rcItem, brush);
        DeleteObject(brush);
        return TRUE;
    }
    if (msg == WM_PRINTCLIENT || msg == WM_ERASEBKGND)
    {
        RECT r;
        HBRUSH brush = CreateSolidBrush((COLORREF)GetWindowLongPtrW(hwnd, GWLP_USERDATA));
        GetClientRect(hwnd, &r);
        FillRect((HDC)wp, &r, brush);
        DeleteObject(brush);
        return 1;
    }
    if (msg == WM_NCCREATE)
    {
        CREATESTRUCTW *cs = (CREATESTRUCTW *)lp;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)cs->lpCreateParams);
    }
    return DefWindowProcW(hwnd,msg,wp,lp);
}
static HWND window(HWND parent, DWORD style, int x, int y, int w, int h, COLORREF color)
{
    HWND hwnd = CreateWindowW(L"Win4WinePrintTest", L"print regression", style,
                    x,y,w,h,parent,NULL,GetModuleHandleW(NULL),(void *)(UINT_PTR)color);
    if (!hwnd) { fprintf(stderr,"CreateWindow failed %lu\n",GetLastError()); failed=1; }
    return hwnd;
}
static void pixel(HDC dc,int x,int y,COLORREF expected,const char *name)
{
    COLORREF actual=GetPixel(dc,x,y);
    if(actual!=expected)
    {
        fprintf(stderr,"%s: pixel %d,%d = %06lx expected %06lx\n",name,x,y,actual,expected);
        failed=1;
    }
}
static void clear(HDC dc)
{
    HBRUSH brush=CreateSolidBrush(untouched);
    RECT all={-20,-20,140,140};
    FillRect(dc,&all,brush);
    DeleteObject(brush);
}
int wmain(void)
{
    WNDCLASSW cls={0};
    HWND root,child,grand,hidden,edge,owned,bordered,overlap,button,label;
    HDC screen=GetDC(NULL),dc=CreateCompatibleDC(screen);
    HBITMAP bitmap=CreateCompatibleBitmap(screen,120,120),previous;
    POINT origin;
    RECT clip,before;
    const COLORREF red=RGB(180,20,30),green=RGB(20,180,30),blue=RGB(20,30,180),yellow=RGB(230,220,10);
    cls.lpfnWndProc=paint_proc; cls.hInstance=GetModuleHandleW(NULL); cls.lpszClassName=L"Win4WinePrintTest";
    if(!RegisterClassW(&cls)||!dc||!bitmap) return 2;
    previous=SelectObject(dc,bitmap); ReleaseDC(NULL,screen);
    root=window(NULL,WS_POPUP|WS_VISIBLE,100,100,80,80,red);
    child=window(root,WS_CHILD|WS_VISIBLE,20,25,30,30,green);
    grand=window(child,WS_CHILD|WS_VISIBLE,5,6,10,11,blue);
    hidden=window(root,WS_CHILD,60,5,10,10,blue);
    overlap=window(root,WS_CHILD|WS_VISIBLE,40,40,20,10,yellow);
    edge=window(root,WS_CHILD|WS_VISIBLE,-5,65,10,20,green);
    owned=window(root,WS_POPUP|WS_VISIBLE,165,165,10,10,yellow);
    bordered=window(root,WS_CHILD|WS_VISIBLE|WS_BORDER,40,60,20,15,blue);
    button=CreateWindowW(L"Button",L"owner draw",WS_CHILD|WS_VISIBLE|BS_OWNERDRAW,
                         60,20,15,15,root,(HMENU)(UINT_PTR)1016,GetModuleHandleW(NULL),NULL);
    label=CreateWindowW(L"Static",L"owner draw",WS_CHILD|WS_VISIBLE|SS_OWNERDRAW,
                        60,40,15,15,root,(HMENU)(UINT_PTR)1017,GetModuleHandleW(NULL),NULL);
    if(failed||!label||!button||!grand||!hidden||!edge||!owned||!bordered||!overlap) return 2;
    if(!SetWindowPos(overlap,HWND_TOP,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE)
       || GetWindow(root,GW_CHILD)!=overlap)
    { fputs("cannot establish overlapping sibling z-order\n",stderr); return 2; }
    SetViewportOrgEx(dc,13,17,NULL);
    IntersectClipRect(dc,-3,-4,90,91);
    GetClipBox(dc,&before);

    clear(dc);
    SendMessageW(root,WM_PRINT,(WPARAM)dc,PRF_CLIENT|PRF_ERASEBKGND);
    pixel(dc,1,1,red,"parent client");
    pixel(dc,21,26,red,"children omitted without flag");
    clear(dc);
    SendMessageW(root,WM_PRINT,(WPARAM)dc,PRF_CLIENT|PRF_ERASEBKGND|PRF_CHILDREN|PRF_NONCLIENT);
    pixel(dc,1,1,red,"parent remains visible");
    pixel(dc,21,26,green,"child offset");
    pixel(dc,26,32,blue,"grandchild offset");
    pixel(dc,41,41,yellow,"overlapping sibling z-order");
    pixel(dc,61,21,yellow,"builtin owner-draw Button at translated viewport");
    pixel(dc,61,41,yellow,"builtin owner-draw Static at translated viewport");
    pixel(dc,61,6,red,"hidden child omitted");
    pixel(dc,1,66,green,"partially clipped child");
    pixel(dc,-1,66,untouched,"child clipped to parent left");
    pixel(dc,1,81,untouched,"child clipped to parent bottom");
    pixel(dc,42,62,blue,"bordered child client offset");
    pixel(dc,40,60,GetSysColor(COLOR_WINDOWFRAME),"bordered child nonclient");
    GetViewportOrgEx(dc,&origin); GetClipBox(dc,&clip);
    if(origin.x!=13||origin.y!=17||!EqualRect(&clip,&before))
    { fputs("caller DC origin/clip was changed\n",stderr); failed=1; }
    clear(dc);
    SendMessageW(root,WM_PRINT,(WPARAM)dc,PRF_CLIENT|PRF_ERASEBKGND|PRF_OWNED);
    pixel(dc,66,66,yellow,"owned window offset");
    ShowWindow(root,SW_HIDE);
    clear(dc);
    SendMessageW(root,WM_PRINT,(WPARAM)dc,PRF_CLIENT|PRF_CHECKVISIBLE|PRF_CHILDREN);
    pixel(dc,1,1,untouched,"invisible parent skipped");
    DestroyWindow(owned); DestroyWindow(root);
    SelectObject(dc,previous); DeleteObject(bitmap); DeleteDC(dc);
    if(!failed) puts("PASS: WM_PRINT child/owned recursion, offsets, clipping, z-order, builtin owner-draw controls, nonclient, visibility, caller DC state");
    return failed ? 1 : 0;
}
