/* Public caption paint checks against public DrawIconEx and Wine stock icons. */
#include <windows.h>
#include <uxtheme.h>
#include <stdio.h>
#include <string.h>

typedef BOOL (WINAPI *draw_nc)(HWND,HDC);
struct testcase {const WCHAR *name;LPCWSTR class_big,class_small,window_big,window_small;BOOL modal;};
static const struct testcase cases[]={
    {L"noicons",0,0,0,0,FALSE},
    {L"classbig",IDI_WARNING,0,0,0,FALSE},
    {L"classsmall",IDI_WARNING,IDI_QUESTION,0,0,FALSE},
    {L"windowbig",IDI_WARNING,IDI_QUESTION,IDI_ERROR,0,FALSE},
    {L"window-small-priority",IDI_WARNING,IDI_QUESTION,IDI_ERROR,IDI_INFORMATION,FALSE},
    {L"modalmissing",0,0,0,0,TRUE}
};
static HICON stock(LPCWSTR id,int size)
{
    return id?LoadImageW(NULL,id,IMAGE_ICON,size,size,LR_SHARED):NULL;
}
int wmain(void)
{
    HMODULE module=LoadLibraryW(L"uxtheme.dll");
    draw_nc draw=(draw_nc)(void*)GetProcAddress(module,"Win4WineDrawNonClient");
    HDC screen=GetDC(NULL),base=CreateCompatibleDC(screen),actual=CreateCompatibleDC(screen),expected=CreateCompatibleDC(screen);
    HBITMAP bitmaps[3];HWND baseline;WNDCLASSEXW wc={0};int i,x,y,failed=0,width=GetSystemMetrics(SM_CXSMICON),height=GetSystemMetrics(SM_CYSMICON);
    int left=GetSystemMetrics(SM_CXFRAME)+2;
    int top=GetSystemMetrics(SM_CYFRAME)+(GetSystemMetrics(SM_CYCAPTION)-1-height)/2;
    if(!draw||!IsThemeActive()){fprintf(stderr,"active theme and external-shell patched Wine required\n");return 2;}
    for(i=0;i<3;i++)bitmaps[i]=CreateCompatibleBitmap(screen,320,180);
    SelectObject(base,bitmaps[0]);SelectObject(actual,bitmaps[1]);SelectObject(expected,bitmaps[2]);
    wc.cbSize=sizeof(wc);wc.lpfnWndProc=DefWindowProcW;wc.hInstance=GetModuleHandleW(NULL);wc.lpszClassName=L"CaptionIconBaseline";
    RegisterClassExW(&wc);
    baseline=CreateWindowExW(WS_EX_DLGMODALFRAME,wc.lpszClassName,L"",WS_OVERLAPPEDWINDOW,100,100,320,180,NULL,NULL,wc.hInstance,NULL);
    if(!draw(baseline,base))return 2;
    for(i=0;i<(int)(sizeof(cases)/sizeof(cases[0]));i++)
    {
        const struct testcase *c=&cases[i];HWND hwnd;HICON icon;int differences=0;
        wc.lpszClassName=c->name;wc.hIcon=stock(c->class_big,32);wc.hIconSm=stock(c->class_small,16);RegisterClassExW(&wc);
        hwnd=CreateWindowExW(c->modal?WS_EX_DLGMODALFRAME:0,c->name,L"",WS_OVERLAPPEDWINDOW,100,100,320,180,NULL,NULL,wc.hInstance,NULL);
        if(c->window_big)SendMessageW(hwnd,WM_SETICON,ICON_BIG,(LPARAM)stock(c->window_big,32));
        if(c->window_small)SendMessageW(hwnd,WM_SETICON,ICON_SMALL,(LPARAM)stock(c->window_small,16));
        BitBlt(expected,0,0,320,180,base,0,0,SRCCOPY);
        icon=stock(c->window_small?c->window_small:c->window_big?c->window_big:c->class_small?c->class_small:c->class_big?c->class_big:c->modal?0:IDI_WINLOGO,
                   c->window_small||(!c->window_big&&(c->class_small||c->class_big))?16:c->window_big?32:width);
        if(icon)DrawIconEx(expected,left,top,icon,width,height,0,NULL,DI_NORMAL);
        if(!draw(hwnd,actual))return 2;
        for(y=top;y<top+height;y++)for(x=left;x<left+width;x++)
            if(GetPixel(expected,x,y)!=GetPixel(actual,x,y))differences++;
        printf("%ls icon=%p mismatched-pixels=%d\n",c->name,icon,differences);
        if(differences)failed++;
        DestroyWindow(hwnd);UnregisterClassW(c->name,wc.hInstance);
    }
    DestroyWindow(baseline);DeleteDC(base);DeleteDC(actual);DeleteDC(expected);
    for(i=0;i<3;i++)DeleteObject(bitmaps[i]);
    ReleaseDC(NULL,screen);FreeLibrary(module);
    return failed?1:0;
}
